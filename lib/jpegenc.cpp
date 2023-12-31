#include <hwy/highway.h>
#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include "block_coding.hpp"
#include "color.hpp"
#include "constants.hpp"
#include "image_chunk.hpp"
#include "huffman_tables.hpp"
#include "jpgheaders.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"

namespace jpegenc {
namespace hn = hwy::HWY_NAMESPACE;

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
  std::vector<std::unique_ptr<uint8_t[], hwy::AlignedFreer>> line_buffer0;
  std::vector<std::unique_ptr<int16_t[], hwy::AlignedFreer>> line_buffer1;
  std::unique_ptr<int16_t[], hwy::AlignedFreer> mcu_buffer;
  std::vector<uint8_t *> yuv0;
  std::vector<int16_t *> yuv1;
  int16_t *mcu;
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
    const size_t bufsize_L = rounded_width * LINES;
    const size_t bufsize_C = rounded_width / scale_x * LINES / scale_y;

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
      line_buffer1[c] = hwy::AllocateAligned<int16_t>(bufsize_C);
    }
    yuv1[0] = line_buffer1[0].get();
    for (int c = 1; c < ncomp_out; ++c) {
      yuv1[c] = line_buffer1[c].get();
    }

    // Prepare mcu-buffers
    const int c = (ncomp_out == 1) ? 1 : 0;
    mcu_buffer  = hwy::AllocateAligned<int16_t>(DCTSIZE2 * scale_x * scale_y + ((DCTSIZE2 * 2) >> c));
    mcu         = mcu_buffer.get();
  }

  void invoke(std::vector<uint8_t> &codestream) {
    jpegenc_hwy::huff_info tab_Y((const uint16_t *)DC_cwd[0], (const uint16_t *)AC_cwd[0],
                                 (const uint8_t *)DC_len[0], (const uint8_t *)AC_len[0]);
    jpegenc_hwy::huff_info tab_C((const uint16_t *)DC_cwd[1], (const uint16_t *)AC_cwd[1],
                                 (const uint8_t *)DC_len[1], (const uint8_t *)AC_len[1]);
    std::vector<int> prev_dc(3, 0);

    // Prepare main-header
    create_scaled_qtable(0, QF, qtable);
    create_scaled_qtable(1, QF, qtable + DCTSIZE2);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);

    //// Encoding
    image.init();
    uint8_t *src = image.get_lines_from(0);

    // Loop of 16 pixels height
    for (int n = 0; n < rounded_height - LINES; n += LINES) {
      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(src, yuv0, rounded_width);
      } else {
        yuv0[0] = src;
      }
      jpegenc_hwy::subsample(yuv0, yuv1, rounded_width, YCCtype);
      jpegenc_hwy::encode_lines(yuv1, mcu, rounded_width, LINES, YCCtype, qtable, prev_dc, tab_Y, tab_C,
                                enc);
      // RST marker insertion, if any
      if (use_RESET) {
        enc.put_RST((n / LINES) % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
      src = image.get_lines_from(n + LINES);
    }
    // Last chunk or leftover (< 16 pixels)
    const int last_mcu_height = (rounded_height % LINES) ? DCTSIZE : LINES;

    if (ncomp == 3) {
      jpegenc_hwy::rgb2ycbcr(src, yuv0, rounded_width);
    } else {
      yuv0[0] = src;
    }
    jpegenc_hwy::subsample(yuv0, yuv1, rounded_width, YCCtype);
    jpegenc_hwy::encode_lines(yuv1, mcu, rounded_width, last_mcu_height, YCCtype, qtable, prev_dc, tab_Y,
                              tab_C, enc);

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