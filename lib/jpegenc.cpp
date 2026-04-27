#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include <algorithm>
#include <cstring>

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
  const int ncomp_out;
  const int num_strips;

  // Single shared buffer set, allocated once and reused across all strips
  // and across invoke() calls.
  hwy::AlignedFreeUniquePtr<uint8_t[]> input_buf;
  std::vector<hwy::AlignedFreeUniquePtr<uint8_t[]>> line_buffer0;
  std::vector<hwy::AlignedFreeUniquePtr<int16_t[]>> line_buffer1;
  std::vector<uint8_t *> yuv0;
  std::vector<int16_t *> yuv1;

  HWY_ALIGN int16_t qtable[DCTSIZE2 * 2];
  bitstream enc;

  jpegenc_hwy::huff_info tab_Y;
  jpegenc_hwy::huff_info tab_C;

  const bool use_RESET;

  static int compute_ncomp_out(int nc, int &ycc_inout) {
    if (nc == 1) ycc_inout = YCC::GRAY;
    return (ycc_inout == YCC::GRAY2) ? 1 : nc;
  }

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
        ncomp_out(compute_ncomp_out(ncomp, YCCtype)),
        num_strips((rounded_height + BUFLINES - 1) / BUFLINES),
        line_buffer0(ncomp),
        line_buffer1(ncomp_out),
        yuv0(ncomp),
        yuv1(ncomp_out),
        qtable{0},
        enc(3000000),
        use_RESET(false) {
    const int scale_x      = YCC_HV[YCCtype][0] >> 4;
    const int scale_y      = YCC_HV[YCCtype][0] & 0xF;
    const size_t bufsize_L = static_cast<size_t>(rounded_width) * BUFLINES;
    const size_t bufsize_C = (scale_x > 0 && scale_y > 0)
                                 ? (static_cast<size_t>(rounded_width) / scale_x) * (BUFLINES / scale_y)
                                 : 0;

    // Input buffer (RGB or grayscale, padded to rounded_width).
    input_buf = hwy::AllocateAligned<uint8_t>(static_cast<size_t>(rounded_width) * BUFLINES * ncomp);

    // Planar Y/Cb/Cr (uint8) line buffers.
    for (int c = 0; c < ncomp; ++c) {
      line_buffer0[c] = hwy::AllocateAligned<uint8_t>(bufsize_L);
      yuv0[c]         = line_buffer0[c].get();
    }

    // Subsampled Y/Cb/Cr (int16) line buffers — Y full size, chroma scaled.
    line_buffer1[0] = hwy::AllocateAligned<int16_t>(bufsize_L);
    yuv1[0]         = line_buffer1[0].get();
    for (int c = 1; c < ncomp_out; ++c) {
      line_buffer1[c] = hwy::AllocateAligned<int16_t>(bufsize_C);
      yuv1[c]         = line_buffer1[c].get();
    }

    // Huffman tables and quantization table — constant for a fixed encoder.
    tab_Y.init<0>();
    tab_C.init<1>();
    create_scaled_qtable(0, QF, qtable);
    create_scaled_qtable(1, QF, qtable + DCTSIZE2);
  }

  void invoke(std::vector<uint8_t> &codestream) {
    image.init();

    // Write main header at the front of the bitstream. After finalize() in a
    // previous invoke(), `enc` was rewound to pos=0, so this overwrites the
    // old header in place.
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);

    std::vector<int> prev_dc{0, 0, 0};

    // Sequentially process each strip directly into `enc`. No per-strip
    // intermediate bitstream — strips concatenate naturally.
    for (int s = 0; s < num_strips; ++s) {
      const int row_off  = s * BUFLINES;
      const int mcu_rows = std::min(BUFLINES, rounded_height - row_off);

      image.get_lines_from(row_off, input_buf.get());

      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(input_buf.get(), yuv0, rounded_width);
      } else {
        yuv0[0] = input_buf.get();
      }
      jpegenc_hwy::subsample(yuv0, yuv1, rounded_width, YCCtype);
      jpegenc_hwy::encode_lines(yuv1, rounded_width, mcu_rows, YCCtype, qtable, prev_dc, tab_Y, tab_C, enc);

      if (use_RESET && s + 1 < num_strips) {
        enc.put_RST(s % 8);
        prev_dc[0] = prev_dc[1] = prev_dc[2] = 0;
      }
    }

    // finalize() flushes pending bits, appends EOI, and returns the entire
    // codestream as a vector. The internal bitstream is rewound to pos=0,
    // ready to be reused on the next invoke().
    codestream = enc.finalize();
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
