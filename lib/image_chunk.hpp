#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <hwy/aligned_allocator.h>

#include "constants.hpp"
#include "ycctype.hpp"
#include "hwy/ops/set_macros-inl.h"

class imchunk {
 private:
  const int width;
  const int height;
  const int ncomp;
  const int rounded_width;
  int origin;
  FILE *g_buf;
  hwy::AlignedFreeUniquePtr<uint8_t[]> buf;
  hwy::AlignedFreeUniquePtr<uint8_t[]> buf_extended;
  int32_t cur_line;

 public:
  explicit imchunk(FILE *imdata, const int p, const int w, const int h, const int nc, const int YCCtype)
      : width(w),
        height(h),
        ncomp(nc),
        rounded_width(round_up(width, HWY_MAX(DCTSIZE * (YCC_HV[YCCtype][0] >> 4), HWY_MAX_BYTES))),
        origin(p),
        g_buf(imdata),
        buf(hwy::AllocateAligned<uint8_t>(static_cast<size_t>(width) * ncomp * LINES)),
        buf_extended(hwy::AllocateAligned<uint8_t>(static_cast<size_t>(rounded_width) * ncomp * LINES)),
        cur_line(0) {}

  void init() { fseek(g_buf, origin, SEEK_SET); }
  uint8_t *get_lines_from(int n) {
    cur_line                 = n;
    const int num_rows       = ((cur_line + LINES) > height) ? height % LINES : LINES;
    const int num_extra_rows = ((cur_line + LINES) > height) ? LINES - height % LINES : 0;
    uint8_t *sp              = buf.get();
    uint8_t *dp              = buf_extended.get();
    fread(sp, sizeof(unsigned char), static_cast<size_t>(width) * num_rows * ncomp, g_buf);
    const int extra_cols = (rounded_width - width) * ncomp;
    for (int i = 0; i < num_rows; ++i) {
      //      fread(dp, sizeof(unsigned char), static_cast<size_t>(width) * ncomp, g_buf);
      memcpy(dp, sp, static_cast<size_t>(width) * ncomp);
      for (int j = 0; j < extra_cols; j += ncomp) {
        dp[ncomp * width + j]     = dp[ncomp * width - 3];
        dp[ncomp * width + j + 1] = dp[ncomp * width - 2];
        dp[ncomp * width + j + 2] = dp[ncomp * width - 1];
      }
      sp += width * ncomp;
      dp += rounded_width * ncomp;
    }
    sp = buf_extended.get() + rounded_width * (num_rows - 1) * ncomp;
    dp = sp + rounded_width * ncomp;
    //    buf_extended.get() + rounded_width *(num_rows)*ncomp;
    for (int i = 0; i < num_extra_rows; ++i) {
      memcpy(dp, sp, static_cast<size_t>(rounded_width) * ncomp);
      dp += rounded_width * ncomp;
    }
    //    memcpy(buf_extended.get(), sp, static_cast<size_t>(rounded_width) * LINES * ncomp);
    return buf_extended.get();
  }

  ~imchunk() = default;
};