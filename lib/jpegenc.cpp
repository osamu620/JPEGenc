#include <hwy/aligned_allocator.h>
#include <jpegenc.hpp>

#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

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
  // ---- Common state, used by both single- and multi-threaded paths ----
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
  const std::size_t num_workers;
  const bool use_RESET;

  HWY_ALIGN int16_t qtable[DCTSIZE2 * 2];
  jpegenc_hwy::huff_info tab_Y;
  jpegenc_hwy::huff_info tab_C;

  // Per-worker buffer set: 1 entry in single-thread mode, num_workers in multi.
  // Recycled across strips and across invoke() calls.
  struct worker_buffers {
    hwy::AlignedFreeUniquePtr<uint8_t[]> input;
    std::vector<hwy::AlignedFreeUniquePtr<uint8_t[]>> line_buffer0;
    std::vector<hwy::AlignedFreeUniquePtr<int16_t[]>> line_buffer1;
    std::vector<uint8_t *> yuv0;
    std::vector<int16_t *> yuv1;
  };
  std::vector<worker_buffers> wbufs;

  // ST: holds header + strip output + EOI in one bitstream.
  // MT: holds the (constant) main header only.
  bitstream enc;
  size_t header_len;  // populated for MT mode (== enc.get_len() after header build)

  // ---- Multi-thread-only state (allocated lazily by the constructor) ----
  std::vector<bitstream> strip_cs;  // one per strip
  std::queue<int> free_idx;
  std::mutex free_mu;
  std::condition_variable free_cv;
  std::unique_ptr<BS::thread_pool<>> pool;

  // ---- Helpers ----
  static int compute_ncomp_out(int nc, int &ycc_inout) {
    if (nc == 1) ycc_inout = YCC::GRAY;
    return (ycc_inout == YCC::GRAY2) ? 1 : nc;
  }

  static std::size_t pick_num_workers(int requested, int strips) {
    std::size_t hc = std::thread::hardware_concurrency();
    if (hc == 0) hc = 1;
    std::size_t n;
    if (requested <= 0) {
      n = hc;  // auto
    } else {
      n = static_cast<std::size_t>(requested);
    }
    n = std::min<std::size_t>(n, hc);
    n = std::min<std::size_t>(n, static_cast<std::size_t>(strips));
    if (n == 0) n = 1;
    return n;
  }

  // Encode one strip into the bitstream `cs`. Used by both modes.
  // The caller is responsible for setting up the input data in wbufs[wb_idx].
  void encode_strip(int s, std::size_t wb_idx, std::vector<int> &prev_dc, bitstream &cs) {
    auto &wb           = wbufs[wb_idx];
    const int row_off  = s * BUFLINES;
    const int mcu_rows = std::min(BUFLINES, rounded_height - row_off);

    if (ncomp == 3) {
      jpegenc_hwy::rgb2ycbcr(wb.input.get(), wb.yuv0, rounded_width);
    } else {
      wb.yuv0[0] = wb.input.get();
    }
    jpegenc_hwy::subsample(wb.yuv0, wb.yuv1, rounded_width, YCCtype);
    jpegenc_hwy::encode_lines(wb.yuv1, rounded_width, mcu_rows, YCCtype, qtable, prev_dc, tab_Y, tab_C, cs);
  }

  // Single-threaded body: reads strips sequentially and writes them, the header,
  // and EOI directly into a single bitstream `enc`. No RST markers.
  void invoke_st(std::vector<uint8_t> &codestream) {
    image.init();
    create_mainheader(width, height, QF, YCCtype, enc, /*use_RESET=*/false);

    std::vector<int> prev_dc{0, 0, 0};
    for (int s = 0; s < num_strips; ++s) {
      image.get_lines_from(s * BUFLINES, wbufs[0].input.get());
      encode_strip(s, 0, prev_dc, enc);
    }

    codestream = enc.finalize();
  }

  // Multi-threaded body: producer (this thread) reads each strip into a free
  // worker buffer and dispatches an encode task. Output is concatenated header
  // + per-strip cs + EOI directly into the caller's vector.
  void invoke_mt(std::vector<uint8_t> &codestream) {
    image.init();

    for (int s = 0; s < num_strips; ++s) {
      int b;
      {
        std::unique_lock<std::mutex> lk(free_mu);
        free_cv.wait(lk, [this] { return !free_idx.empty(); });
        b = free_idx.front();
        free_idx.pop();
      }
      image.get_lines_from(s * BUFLINES, wbufs[b].input.get());

      pool->detach_task([this, s, b]() {
        std::vector<int> prev_dc{0, 0, 0};
        encode_strip(s, static_cast<std::size_t>(b), prev_dc, strip_cs[s]);
        if (s + 1 < num_strips) {
          strip_cs[s].put_RST(s % 8);
        }
        {
          std::lock_guard<std::mutex> lk(free_mu);
          free_idx.push(b);
        }
        free_cv.notify_one();
      });
    }
    pool->wait();

    // Single-pass concat: header + every strip + EOI.
    std::vector<size_t> strip_lens(num_strips);
    size_t total = header_len + 2;  // +EOI
    for (int s = 0; s < num_strips; ++s) {
      strip_lens[s] = strip_cs[s].get_len();
      total += strip_lens[s];
    }
    codestream.resize(total);
    uint8_t *out = codestream.data();
    size_t pos   = 0;
    std::memcpy(out + pos, enc.get_stream()->get_buf(), header_len);
    pos += header_len;
    for (int s = 0; s < num_strips; ++s) {
      std::memcpy(out + pos, strip_cs[s].get_stream()->get_buf(), strip_lens[s]);
      pos += strip_lens[s];
    }
    out[pos++] = static_cast<uint8_t>(EOI >> 8);
    out[pos++] = static_cast<uint8_t>(EOI & 0xFF);
  }

 public:
  jpeg_encoder_impl(im_info &inimg, int &qf, int &ycc, int requested_threads)
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
        num_workers(pick_num_workers(requested_threads, num_strips)),
        use_RESET(num_workers > 1),
        qtable{0},
        // ST: pre-size enc for the whole codestream so it doesn't doubling-grow.
        // MT: enc just holds the constant main header.
        enc(num_workers == 1 ? 3000000 : 8192),
        header_len(0) {
    const int scale_x      = YCC_HV[YCCtype][0] >> 4;
    const int scale_y      = YCC_HV[YCCtype][0] & 0xF;
    const size_t bufsize_L = static_cast<size_t>(rounded_width) * BUFLINES;
    const size_t bufsize_C = (scale_x > 0 && scale_y > 0)
                                 ? (static_cast<size_t>(rounded_width) / scale_x) * (BUFLINES / scale_y)
                                 : 0;

    // Per-worker buffer sets.
    wbufs.resize(num_workers);
    for (std::size_t w = 0; w < num_workers; ++w) {
      auto &wb = wbufs[w];
      wb.input = hwy::AllocateAligned<uint8_t>(static_cast<size_t>(rounded_width) * BUFLINES * ncomp);
      wb.line_buffer0.reserve(ncomp);
      wb.yuv0.reserve(ncomp);
      for (int c = 0; c < ncomp; ++c) {
        wb.line_buffer0.emplace_back(hwy::AllocateAligned<uint8_t>(bufsize_L));
        wb.yuv0.push_back(wb.line_buffer0.back().get());
      }
      wb.line_buffer1.reserve(ncomp_out);
      wb.yuv1.reserve(ncomp_out);
      wb.line_buffer1.emplace_back(hwy::AllocateAligned<int16_t>(bufsize_L));
      wb.yuv1.push_back(wb.line_buffer1.back().get());
      for (int c = 1; c < ncomp_out; ++c) {
        wb.line_buffer1.emplace_back(hwy::AllocateAligned<int16_t>(bufsize_C));
        wb.yuv1.push_back(wb.line_buffer1.back().get());
      }
    }

    // Constant tables.
    tab_Y.init<0>();
    tab_C.init<1>();
    create_scaled_qtable(0, QF, qtable);
    create_scaled_qtable(1, QF, qtable + DCTSIZE2);

    // Mode-specific allocations.
    if (num_workers > 1) {
      // Build the constant main header once into `enc`.
      create_mainheader(width, height, QF, YCCtype, enc, use_RESET);
      header_len = enc.get_len();

      // Per-strip output streams; capacity grows on demand and persists across invokes.
      strip_cs.resize(num_strips);
      for (auto &cs : strip_cs) cs.init(1024);

      // Free-buffer queue + persistent thread pool.
      for (std::size_t w = 0; w < num_workers; ++w) free_idx.push(static_cast<int>(w));
      pool = std::make_unique<BS::thread_pool<>>(num_workers);
    }
  }

  void invoke(std::vector<uint8_t> &codestream) {
    if (num_workers == 1) {
      invoke_st(codestream);
    } else {
      invoke_mt(codestream);
    }
  }

  ~jpeg_encoder_impl() = default;
};

/**********************************************************************************************************************/
// Public interface
/**********************************************************************************************************************/
jpeg_encoder::jpeg_encoder(im_info &inimg, int &QF, int &YCCtype, int num_threads) {
  this->impl = std::make_unique<jpeg_encoder_impl>(inimg, QF, YCCtype, num_threads);
}

void jpeg_encoder::invoke() { this->impl->invoke(this->codestream); }

std::vector<uint8_t> jpeg_encoder::get_codestream() { return std::move(this->codestream); }

jpeg_encoder::~jpeg_encoder() = default;
}  // namespace jpegenc
