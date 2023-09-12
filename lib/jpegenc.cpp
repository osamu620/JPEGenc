#include <hwy/highway.h>
#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include "block_coding.hpp"
#include "color.hpp"
#include "constants.hpp"
#include "dct.hpp"
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
  int QF;
  int YCCtype;
  const int rounded_width;
  const int rounded_height;
  std::vector<std::unique_ptr<int16_t[], hwy::AlignedFreer>> line_buffer;
  std::vector<int16_t *> yuv;
  HWY_ALIGN int qtable_L[64];
  HWY_ALIGN int qtable_C[64];
  bitstream enc;
  bool use_RESET;

 public:
  jpeg_encoder_impl(im_info &inimg, int &qf, int &ycc)
      : image(inimg.data, inimg.width, inimg.height, inimg.nc, ycc),
        width(inimg.width),
        height(inimg.height),
        ncomp(inimg.nc),
        QF(qf),
        YCCtype(ycc),
        rounded_width(round_up(inimg.width, DCTSIZE * (YCC_HV[YCCtype][0] >> 4))),
        rounded_height(round_up(inimg.height, DCTSIZE * (YCC_HV[YCCtype][0] & 0xF))),
        line_buffer(ncomp),
        yuv(ncomp),
        qtable_L{0},
        qtable_C{0},
        use_RESET(false) {
    int nc = inimg.nc;
    if (nc == 1) {
      YCCtype = YCC::GRAY;
    }
    nc                     = (YCCtype == YCC::GRAY2) ? 1 : nc;
    const int scale_x      = YCC_HV[YCCtype][0] >> 4;
    const int scale_y      = YCC_HV[YCCtype][0] & 0xF;
    const size_t bufsize_L = rounded_width * LINES;
    const size_t bufsize_C = rounded_width / scale_x * LINES / scale_y;

    // Prepare line-buffers
    line_buffer[0] = hwy::AllocateAligned<int16_t>(bufsize_L);
    for (size_t c = 1; c < line_buffer.size(); ++c) {
      line_buffer[c] = hwy::AllocateAligned<int16_t>(bufsize_C);
    }

    yuv[0] = line_buffer[0].get();
    for (int c = 1; c < nc; ++c) {
      yuv[c] = line_buffer[c].get();
    }
  }

  void invoke(std::vector<uint8_t> &codestream) {
    jpegenc_hwy::huff_info tab_Y((const uint16_t *)DC_cwd[0], (const uint16_t *)AC_cwd[0],
                                 (const uint8_t *)DC_len[0], (const uint8_t *)AC_len[0]);
    jpegenc_hwy::huff_info tab_C((const uint16_t *)DC_cwd[1], (const uint16_t *)AC_cwd[1],
                                 (const uint8_t *)DC_len[1], (const uint8_t *)AC_len[1]);
    std::vector<int> prev_dc(3, 0);

    // Prepare main-header
    create_qtable(0, QF, qtable_L);
    create_qtable(1, QF, qtable_C);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);

    //// Encoding
    int n;
    uint8_t *src;
    // Loop of 16 pixels height
    for (n = 0; n < rounded_height - LINES; n += LINES) {
      src = image.get_lines_from(n);
      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(src, rounded_width);
      }
      jpegenc_hwy::subsample(src, yuv, rounded_width, YCCtype);
      jpegenc_hwy::dct2(yuv, rounded_width, LINES, YCCtype);
      jpegenc_hwy::quantize(yuv, rounded_width, LINES, YCCtype, qtable_L, qtable_C);
      jpegenc_hwy::Encode_MCUs(yuv, rounded_width, LINES, YCCtype, prev_dc, tab_Y, tab_C, enc);
      if (use_RESET) {
        enc.put_RST(n % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
    }
    // Last chunk or leftover (< 16 pixels)
    int last_mcu_height = LINES;
    if (rounded_height % LINES) {
      last_mcu_height = DCTSIZE;
    }
    src = image.get_lines_from(n);
    if (ncomp == 3) {
      jpegenc_hwy::rgb2ycbcr(src, rounded_width);
    }
    jpegenc_hwy::subsample(src, yuv, rounded_width, YCCtype);
    jpegenc_hwy::dct2(yuv, rounded_width, last_mcu_height, YCCtype);
    jpegenc_hwy::quantize(yuv, rounded_width, last_mcu_height, YCCtype, qtable_L, qtable_C);
    jpegenc_hwy::Encode_MCUs(yuv, rounded_width, last_mcu_height, YCCtype, prev_dc, tab_Y, tab_C, enc);

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