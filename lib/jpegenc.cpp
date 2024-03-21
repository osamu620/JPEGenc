#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include "block_coding.hpp"
#include "color.hpp"
#include "constants.hpp"
#include "image_chunk.hpp"
#include "jpgheaders.hpp"
#include "ycctype.hpp"

namespace jpegenc {

class jpeg_encoder_impl {
 private:
  imchunk image;
  const int width;
  const int height;
  const int ncomp;
  const int QF;
  int YCCtype;
  const int rounded_width;
  const int rounded_height;
  std::vector<hwy::AlignedFreeUniquePtr<uint8_t[]>> line_buffer0;
  std::vector<hwy::AlignedFreeUniquePtr<int16_t[]>> line_buffer1;
  std::vector<uint8_t *> yuv0;
  std::vector<int16_t *> yuv1;
  HWY_ALIGN int16_t qtable[DCTSIZE2 * 2];
  bitstream enc;
  const bool use_RESET;

 public:
  jpeg_encoder_impl(im_info &inimg, int &qf, int &ycc)
      : image(inimg.data, inimg.pos, inimg.width, inimg.height, inimg.nc, ycc),
        width(inimg.width),
        height(inimg.height),
        ncomp(inimg.nc),
        QF(qf),
        YCCtype(ycc),
        rounded_width(round_up(width, HWY_MAX(DCTSIZE * (YCC_HV[YCCtype][0] >> 4), HWY_MAX_BYTES))),
        rounded_height(round_up(height, DCTSIZE * (YCC_HV[YCCtype][0] & 0xF))),
        line_buffer0(ncomp),
        line_buffer1(ncomp),
        yuv0(ncomp),
        yuv1(ncomp),
        qtable{0},
        enc(3000000),
        use_RESET(false) {
    int ncomp_out = inimg.nc;
    if (ncomp_out == 1) {
      YCCtype = YCC::GRAY;
    }
    ncomp_out              = (YCCtype == YCC::GRAY2) ? 1 : ncomp_out;
    const int scale_x      = YCC_HV[YCCtype][0] >> 4;
    const int scale_y      = YCC_HV[YCCtype][0] & 0xF;
    const size_t bufsize_L = rounded_width * BUFLINES;
    const size_t bufsize_C = rounded_width / scale_x * BUFLINES / scale_y;

    // Prepare line-buffers
    line_buffer0[0] = hwy::AllocateAligned<uint8_t>(bufsize_L);
    for (int c = 1; c < ncomp; ++c) {
      line_buffer0[c] = hwy::AllocateAligned<uint8_t>(bufsize_L);
    }
    yuv0[0] = line_buffer0[0].get();
    for (int c = 1; c < ncomp; ++c) {
      yuv0[c] = line_buffer0[c].get();
    }

    line_buffer1[0] = hwy::AllocateAligned<int16_t>(bufsize_L);
    for (size_t c = 1; c < line_buffer1.size(); ++c) {
      // subsampled chroma
      line_buffer1[c] = hwy::AllocateAligned<int16_t>(bufsize_C);
    }
    yuv1[0] = line_buffer1[0].get();
    for (int c = 1; c < ncomp_out; ++c) {
      yuv1[c] = line_buffer1[c].get();
    }
  }

  void invoke(std::vector<uint8_t> &codestream) {
    jpegenc_hwy::huff_info tab_Y, tab_C;
    tab_Y.init<0>();
    tab_C.init<1>();
    std::vector<int> prev_dc(3, 0);

    // Prepare main-header
    create_scaled_qtable(0, QF, qtable);
    create_scaled_qtable(1, QF, qtable + DCTSIZE2);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);

    //// Encoding
    image.init();
    uint8_t *src = image.get_lines_from(0);

    // Loop of 16 pixels height
    for (int n = 0; n < rounded_height - BUFLINES; n += BUFLINES) {
      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(src, yuv0, rounded_width);
      } else {
        yuv0[0] = src;
      }
      jpegenc_hwy::subsample(yuv0, yuv1, rounded_width, YCCtype);
      jpegenc_hwy::encode_lines(yuv1, rounded_width, BUFLINES, YCCtype, qtable, prev_dc, tab_Y, tab_C, enc);
      // RST marker insertion, if any
      if (use_RESET) {
        enc.put_RST((n / BUFLINES) % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
      src = image.get_lines_from(n + BUFLINES);
    }
    // Last chunk or leftover (< 16 pixels)
    const int last_mcu_height = (rounded_height % BUFLINES) ? DCTSIZE : BUFLINES;

    if (ncomp == 3) {
      jpegenc_hwy::rgb2ycbcr(src, yuv0, rounded_width);
    } else {
      yuv0[0] = src;
    }
    jpegenc_hwy::subsample(yuv0, yuv1, rounded_width, YCCtype);
    jpegenc_hwy::encode_lines(yuv1, rounded_width, last_mcu_height, YCCtype, qtable, prev_dc, tab_Y, tab_C,
                              enc);

    // Finalize codestream
    codestream = const_cast<std::vector<uint8_t> &&>(enc.finalize());
  }

  ~jpeg_encoder_impl() = default;
};

/**********************************************************************************************************************/
// Public interface
/**********************************************************************************************************************/
jpeg_encoder::jpeg_encoder(im_info &inimg, int &QF, int &YCCtype) {
  this->impl = std::make_unique<jpeg_encoder_impl>(inimg, QF, YCCtype);
}

void jpeg_encoder::invoke() { this->impl->invoke(this->codestream); }

std::vector<uint8_t> jpeg_encoder::get_codestream() { return std::move(this->codestream); }

jpeg_encoder::~jpeg_encoder() = default;
}  // namespace jpegenc