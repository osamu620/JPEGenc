#include <hwy/highway.h>
#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include "block_coding.hpp"
#include "color.hpp"
#include "dct.hpp"
#include "image_chunk.hpp"
#include "huffman_tables.hpp"
#include "jpgheaders.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"

#if defined(_MSC_VER)
  #define JPEGENC_EXPORT __declspec(dllexport)
#else
  #define JPEGENC_EXPORT
#endif
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
  std::vector<std::unique_ptr<int16_t[], hwy::AlignedFreer>> line_buffer;
  std::vector<int16_t *> yuv;
  HWY_ALIGN int qtable_L[64];
  HWY_ALIGN int qtable_C[64];
  bitstream enc;
  bool use_RESET;

 public:
  jpeg_encoder_impl(im_info &inimg, int &qf, int &ycc)
      : image(inimg.data, inimg.width, inimg.height, inimg.nc),
        width(inimg.width),
        height(inimg.height),
        ncomp(inimg.nc),
        QF(qf),
        YCCtype(ycc),
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
    const size_t bufsize_L = width * LINES;
    const size_t bufsize_C = width / scale_x * LINES / scale_y;

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

    // Encoding
    for (int n = 0; n < height; n += LINES) {
      uint8_t *src = image.get_lines_from(n);
      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(src, width);
      }
      jpegenc_hwy::subsample(src, yuv, width, YCCtype);
      jpegenc_hwy::dct2(yuv, width, YCCtype);
      jpegenc_hwy::quantize(yuv, qtable_L, qtable_C, width, YCCtype);
      jpegenc_hwy::Encode_MCUs(yuv, width, YCCtype, prev_dc, tab_Y, tab_C, enc);
      if (use_RESET) {
        enc.put_RST(n % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
    }

    // Finalize codestream
    codestream = const_cast<std::vector<uint8_t> &&>(enc.finalize());
  }

  ~jpeg_encoder_impl() = default;
};

/**********************************************************************************************************************/
// Public interface
/**********************************************************************************************************************/
JPEGENC_EXPORT jpeg_encoder::jpeg_encoder(im_info &inimg, int &QF, int &YCCtype) {
  this->impl = std::make_unique<jpeg_encoder_impl>(inimg, QF, YCCtype);
}

JPEGENC_EXPORT void jpeg_encoder::invoke() { this->impl->invoke(this->codestream); }

JPEGENC_EXPORT std::vector<uint8_t> jpeg_encoder::get_codestream() { return std::move(this->codestream); }

JPEGENC_EXPORT jpeg_encoder::~jpeg_encoder() = default;
}  // namespace jpegenc