#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <hwy/aligned_allocator.h>

#include "constants.hpp"
#include "ycctype.hpp"

class imchunk {
 private:
  const int width;
  const int height;
  const int ncomp;
  const int rounded_width;
  uint8_t *origin;
  uint8_t *g_buf;
  std::unique_ptr<uint8_t[], hwy::AlignedFreer> buf;
  int32_t cur_line;

 public:
  explicit imchunk(uint8_t *imdata, const int w, const int h, const int nc, const int YCCtype)
      : width(w),
        height(h),
        ncomp(nc),
        rounded_width(round_up(width, DCTSIZE * (YCC_HV[YCCtype][0] >> 4))),
        origin(imdata),
        g_buf(imdata),
        buf(hwy::AllocateAligned<uint8_t>(static_cast<size_t>(rounded_width) * ncomp * LINES)),
        cur_line(0) {}

  void init() { g_buf = origin; }
  uint8_t *get_lines_from(int n) {
    cur_line                 = n;
    const int num_rows       = ((cur_line + LINES) > height) ? height % LINES : LINES;
    const int num_extra_rows = ((cur_line + LINES) > height) ? LINES - height % LINES : 0;
    //    uint8_t *sp              = g_buf + width * cur_line * ncomp;
    uint8_t *dp          = buf.get();
    const int extra_cols = (rounded_width - width) * ncomp;
    for (int i = 0; i < num_rows; ++i) {
      memcpy(dp, g_buf, static_cast<size_t>(width) * ncomp);
      for (int j = 0; j < extra_cols; j += ncomp) {
        dp[ncomp * width + j]     = dp[ncomp * width - 3];
        dp[ncomp * width + j + 1] = dp[ncomp * width - 2];
        dp[ncomp * width + j + 2] = dp[ncomp * width - 1];
      }
      g_buf += width * ncomp;
      dp += rounded_width * ncomp;
    }
    uint8_t *sp = buf.get() + rounded_width * (num_rows - 1) * ncomp;
    dp          = buf.get() + rounded_width * (num_rows)*ncomp;
    for (int i = 0; i < num_extra_rows; ++i) {
      memcpy(dp, sp, static_cast<size_t>(rounded_width) * ncomp);
      dp += rounded_width * ncomp;
    }
    //    memcpy(buf.get(), sp, static_cast<size_t>(rounded_width) * LINES * ncomp);
    return buf.get();
  }

  ~imchunk() = default;
};