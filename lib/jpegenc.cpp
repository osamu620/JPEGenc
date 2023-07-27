#include <hwy/highway.h>
#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

// #include "aligned_unique_ptr.hpp"
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
  int QF;
  int YCCtype;
  std::vector<std::unique_ptr<int16_t[], hwy::AlignedFreer>> line_buffer;
  //  std::vector<unique_ptr_aligned<int16_t>> line_buffer;
  std::vector<int16_t *> yuv;
  HWY_ALIGN int qtable_L[64];
  HWY_ALIGN int qtable_C[64];
  std::vector<int> prev_dc;
  bitstream enc;
  bool use_RESET;

 public:
  jpeg_encoder_impl(const std::string &infile, int &qf, int &ycc)
      : image(infile),
        width(image.get_width()),
        height(image.get_height()),
        QF(qf),
        YCCtype(ycc),
        line_buffer(image.get_num_comps()),
        yuv(image.get_num_comps()),
        qtable_L{0},
        qtable_C{0},
        prev_dc(3, 0),
        use_RESET(false) {
    int nc = image.get_num_comps();
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
    //    hn::ScalableTag<uint8_t> u8;
    //    const size_t Nalign = hn::Lanes(u8);
    //    line_buffer[0]      = aligned_uptr<int16_t>(Nalign, bufsize_L);
    //    for (size_t c = 1; c < line_buffer.size(); ++c) {
    //      line_buffer[c] = aligned_uptr<int16_t>(Nalign, bufsize_C);
    //    }

    yuv[0] = line_buffer[0].get();
    for (int c = 1; c < nc; ++c) {
      yuv[c] = line_buffer[c].get();
    }

    create_qtable(0, QF, qtable_L);
    create_qtable(1, QF, qtable_C);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);
  }

  void invoke(std::vector<uint8_t> &codestream) {
    jpegenc_hwy::huff_info tab_Y((const uint16_t *)DC_cwd[0], (const uint16_t *)AC_cwd[0],
                                 (const uint8_t *)DC_len[0], (const uint8_t *)AC_len[0]);
    jpegenc_hwy::huff_info tab_C((const uint16_t *)DC_cwd[1], (const uint16_t *)AC_cwd[1],
                                 (const uint8_t *)DC_len[1], (const uint8_t *)AC_len[1]);
    for (int n = 0; n < height / LINES; ++n) {
      uint8_t *src = image.get_lines_from(n);
      if (image.get_num_comps() == 3) {
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

  [[nodiscard]] int32_t get_width() const { return width; }

  [[nodiscard]] int32_t get_height() const { return height; }

  ~jpeg_encoder_impl() = default;
};

/**********************************************************************************************************************/
// Public interface
/**********************************************************************************************************************/
JPEGENC_EXPORT jpeg_encoder::jpeg_encoder(const std::string &infile, int &QF, int &YCCtype) {
  this->impl = std::make_unique<jpeg_encoder_impl>(infile, QF, YCCtype);
}

JPEGENC_EXPORT int32_t jpeg_encoder::get_width() { return this->impl->get_width(); }

JPEGENC_EXPORT int32_t jpeg_encoder::get_height() { return this->impl->get_height(); }

JPEGENC_EXPORT void jpeg_encoder::invoke() { this->impl->invoke(this->codestream); }

JPEGENC_EXPORT std::vector<uint8_t> jpeg_encoder::get_codestream() { return std::move(this->codestream); }

JPEGENC_EXPORT jpeg_encoder::~jpeg_encoder() = default;
}  // namespace jpegenc