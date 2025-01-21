#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include "block_coding.hpp"
#include "color.hpp"
#include "constants.hpp"
#include "image_chunk.hpp"
#include "jpgheaders.hpp"
#include "ycctype.hpp"

#include "BS_thread_pool.hpp"

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

  class enc_object {
   public:
    hwy::AlignedFreeUniquePtr<uint8_t[]> buf;
    uint8_t *src;
    std::vector<hwy::AlignedFreeUniquePtr<uint8_t[]>> line_buffer0;
    std::vector<hwy::AlignedFreeUniquePtr<int16_t[]>> line_buffer1;
    std::vector<uint8_t *> yuv0;
    std::vector<int16_t *> yuv1;
    bitstream cs;
    std::vector<int> prev_dc;
    enc_object() : src(nullptr), cs(8192), prev_dc(3, 0) {};
  };

  std::vector<enc_object> enc_objects;

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
        enc_objects(rounded_height / BUFLINES),
        line_buffer0(ncomp),
        line_buffer1(ncomp),
        yuv0(ncomp),
        yuv1(ncomp),
        qtable{0},
        enc(3000000),
        use_RESET(true) {
    int ncomp_out = inimg.nc;
    if (ncomp_out == 1) {
      YCCtype = YCC::GRAY;
    }
    ncomp_out              = (YCCtype == YCC::GRAY2) ? 1 : ncomp_out;
    const int scale_x      = YCC_HV[YCCtype][0] >> 4;
    const int scale_y      = YCC_HV[YCCtype][0] & 0xF;
    const size_t bufsize_L = rounded_width * BUFLINES;
    const size_t bufsize_C = rounded_width / scale_x * BUFLINES / scale_y;

    for (int i = 0; i < rounded_height / BUFLINES; i++) {
      // Prepare line-buffers
      enc_objects[i].line_buffer0.emplace_back(hwy::AllocateAligned<uint8_t>(bufsize_L));
      for (int c = 1; c < ncomp; ++c) {
        enc_objects[i].line_buffer0.emplace_back(hwy::AllocateAligned<uint8_t>(bufsize_L));
      }
      enc_objects[i].yuv0.push_back(enc_objects[i].line_buffer0[0].get());
      for (int c = 1; c < ncomp; ++c) {
        enc_objects[i].yuv0.push_back(enc_objects[i].line_buffer0[c].get());
      }

      enc_objects[i].line_buffer1.emplace_back(hwy::AllocateAligned<int16_t>(bufsize_L));
      for (size_t c = 1; c < ncomp_out; ++c) {
        // subsampled chroma
        enc_objects[i].line_buffer1.emplace_back(hwy::AllocateAligned<int16_t>(bufsize_C));
      }
      enc_objects[i].yuv1.push_back(enc_objects[i].line_buffer1[0].get());
      for (int c = 1; c < ncomp_out; ++c) {
        enc_objects[i].yuv1.push_back(enc_objects[i].line_buffer1[c].get());
      }
    }
  }

  void invoke(std::vector<uint8_t> &codestream) {
    jpegenc_hwy::huff_info tab_Y, tab_C;
    tab_Y.init<0>();
    tab_C.init<1>();

    image.init();
    size_t num_unit = 0;
    for (int n = 0; n < rounded_height - BUFLINES; n += BUFLINES, num_unit++) {
      enc_objects[num_unit].buf = hwy::AllocateAligned<uint8_t>(rounded_width * BUFLINES * ncomp);
      enc_objects[num_unit].src = enc_objects[num_unit].buf.get();
      for (int i = 0; i < rounded_width * BUFLINES * ncomp; i++) {
        enc_objects[num_unit].src[i] = 0;
      }
      image.get_lines_from(n, enc_objects[num_unit].src);
    }
    const int last_mcu_height = (rounded_height % BUFLINES) ? DCTSIZE : BUFLINES;
    enc_objects[num_unit].buf = hwy::AllocateAligned<uint8_t>(rounded_width * BUFLINES * ncomp);
    enc_objects[num_unit].src = enc_objects[num_unit].buf.get();
    image.get_lines_from(rounded_height - BUFLINES, enc_objects[num_unit].src);

    // Prepare main-header
    create_scaled_qtable(0, QF, qtable);
    create_scaled_qtable(1, QF, qtable + DCTSIZE2);
    create_mainheader(width, height, QF, YCCtype, enc, use_RESET);

    // Loop of 16 pixels height
    BS::thread_pool pool(std::thread::hardware_concurrency());

    // const BS::multi_future<void> loop_future =
    pool.detach_loop(0, num_unit, [this, &tab_Y, &tab_C](const int n) {
      if (ncomp == 3) {
        jpegenc_hwy::rgb2ycbcr(enc_objects[n].src, enc_objects[n].yuv0, rounded_width);
      } else {
        enc_objects[n].yuv0[0] = enc_objects[n].src;
      }
      jpegenc_hwy::subsample(enc_objects[n].yuv0, enc_objects[n].yuv1, rounded_width, YCCtype);
      jpegenc_hwy::encode_lines(enc_objects[n].yuv1, rounded_width, BUFLINES, YCCtype, qtable,
                                enc_objects[n].prev_dc, tab_Y, tab_C, enc_objects[n].cs);
      // RST marker insertion, if any
      if (use_RESET) {
        enc_objects[n].cs.put_RST((n * 16 / BUFLINES) % 8);
        enc_objects[n].prev_dc[0] = enc_objects[n].prev_dc[1] = enc_objects[n].prev_dc[2] = 0;
      }
    });
    // loop_future.wait();
    pool.wait();

    // last chunk
    if (ncomp == 3) {
      jpegenc_hwy::rgb2ycbcr(enc_objects[num_unit].src, enc_objects[num_unit].yuv0, rounded_width);
    } else {
      enc_objects[num_unit].yuv0[0] = enc_objects[num_unit].src;
    }
    jpegenc_hwy::subsample(enc_objects[num_unit].yuv0, enc_objects[num_unit].yuv1, rounded_width, YCCtype);
    jpegenc_hwy::encode_lines(enc_objects[num_unit].yuv1, rounded_width, last_mcu_height, YCCtype, qtable,
                              enc_objects[num_unit].prev_dc, tab_Y, tab_C, enc_objects[num_unit].cs);
    enc_objects[num_unit].prev_dc[0] = enc_objects[num_unit].prev_dc[1] = enc_objects[num_unit].prev_dc[2] =
        0;

    // Finalize codestream
    size_t total_length = 0;
    for (auto &i : enc_objects) {
      total_length += i.cs.get_len();
    }
    size_t header_len = enc.get_len();
    bitstream final(total_length + header_len);
    size_t tmp_len = 0;
    uint8_t *p     = final.get_stream()->get_buf();
    memcpy(p + tmp_len, enc.get_stream()->get_buf(), header_len);
    tmp_len += header_len;
    for (auto &i : enc_objects) {
      size_t len     = i.cs.get_len();
      auto buf_local = i.cs.get_stream();
      memcpy(p + tmp_len, buf_local->get_buf(), len);
      tmp_len += len;
    }
    final.get_stream()->put_pos(tmp_len);
    codestream = const_cast<std::vector<uint8_t> &&>(final.finalize());
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