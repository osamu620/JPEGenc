#include "../include/jpegenc.hpp"

#include <utility>
#include "aligned_unique_ptr.hpp"
#include "block_coding.hpp"
#include "color.hpp"
#include "dct.hpp"
#include "image_chunk.hpp"
#include "jpgheaders.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"

namespace jpegenc {
class jpeg_encoder_impl {
 private:
  imchunk image;
  const int width;
  const int height;
  int QF;
  int YCCtype;
  std::vector<unique_ptr_aligned<int16_t>> line_buffer;
  std::vector<int16_t *> yuv;
  alignas(32) int qtable_L[64];
  alignas(32) int qtable_C[64];
  std::vector<int> prev_dc;
  bitstream enc;
  bool use_RESET;

 public:
  jpeg_encoder_impl(const std::string infile, int &qf, int &ycc)
      : image(infile),
        width(image.get_width()),
        height(image.get_height()),
        QF(qf),
        YCCtype(ycc),
        line_buffer(image.get_num_comps()),
        yuv(image.get_num_comps()),
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
    line_buffer[0] = aligned_uptr<int16_t>(32, bufsize_L);
    for (int c = 1; c < line_buffer.size(); ++c) {
      line_buffer[c] = aligned_uptr<int16_t>(32, bufsize_C);
    }

    yuv[0] = line_buffer[0].get();
    for (int c = 1; c < nc; ++c) {
      yuv[c] = line_buffer[c].get();
    }

    create_qtable(0, QF, qtable_L);
    create_qtable(1, QF, qtable_C);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);
  }

  void invoke(std::vector<uint8_t> &codestream) {
    for (int n = 0; n < height / LINES; ++n) {
      uint8_t *src = image.get_lines_from(n);
      if (image.get_num_comps() == 3) {
        rgb2ycbcr(src, width);
      }
      subsample(src, yuv, width, YCCtype);
      dct2(yuv, width, YCCtype);
      quantize(yuv, qtable_L, qtable_C, width, YCCtype);
      Encode_MCUs(yuv, width, YCCtype, prev_dc, enc);
      if (use_RESET) {
        enc.put_RST(n % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
    }
    // Finalize codestream
    codestream = const_cast<std::vector<uint8_t> &&>(enc.finalize());
  }

  int get_width() const { return width; }
  int get_height() const { return height; }

  ~jpeg_encoder_impl() = default;
};

jpeg_encoder::jpeg_encoder(const std::string &infile, int &QF, int &YCCtype, int &width, int &height) {
  this->impl = std::make_unique<jpeg_encoder_impl>(infile, QF, YCCtype);
  width      = this->impl->get_width();
  height     = this->impl->get_height();
}

void jpeg_encoder::invoke() { this->impl->invoke(this->codestream); }

std::vector<uint8_t> jpeg_encoder::get_codestream() { return std::move(this->codestream); }
jpeg_encoder::~jpeg_encoder() = default;
}  // namespace jpegenc