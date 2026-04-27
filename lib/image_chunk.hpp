#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <hwy/aligned_allocator.h>

#include "constants.hpp"
#include "ycctype.hpp"
// #include "hwy/ops/set_macros-inl.h"

class imchunk {
 private:
  const int height;
  const int ncomp;
  const size_t in_width;
  const size_t out_width;
  int origin;
  FILE *file;
  hwy::AlignedFreeUniquePtr<uint8_t[]> buf;
  int32_t cur_line;

 public:
  explicit imchunk(FILE *imdata, const int p, const int w, const int h, const int nc, const int YCCtype)
      : height(h),
        ncomp(nc),
        in_width(w * ncomp),
        out_width(round_up(w, HWY_MAX(DCTSIZE * (YCC_HV[YCCtype][0] >> 4), HWY_MAX_BYTES)) * ncomp),
        origin(p),
        file(imdata),
        buf(hwy::AllocateAligned<uint8_t>(out_width * BUFLINES)),
        cur_line(0) {}

  void init() { fseek(file, origin, SEEK_SET); }
  uint8_t *get_lines_from(int n) {
    cur_line                 = n;
    const int num_rows       = ((cur_line + BUFLINES) > height) ? height % BUFLINES : BUFLINES;
    const int num_extra_rows = ((cur_line + BUFLINES) > height) ? BUFLINES - height % BUFLINES : 0;
    uint8_t *dp              = buf.get();
    // Read all rows packed at the start of buf, then expand to strided layout in place
    // (bottom-up so we never overwrite source bytes still needed). Capacity is
    // out_width * BUFLINES >= in_width * BUFLINES.
    (void)fread(dp, sizeof(uint8_t), in_width * num_rows, file);
    const size_t extra_cols = out_width - in_width;
    for (int i = num_rows - 1; i >= 0; --i) {
      uint8_t *src = dp + static_cast<size_t>(i) * in_width;
      uint8_t *dst = dp + static_cast<size_t>(i) * out_width;
      if (dst != src) {
        memmove(dst, src, in_width);
      }
      uint8_t *p = dst + in_width;
      for (size_t j = 0; j < extra_cols; j += ncomp) {
        p[j] = p[-3];
        if (j + 1 < extra_cols) p[j + 1] = p[-2];
        if (j + 2 < extra_cols) p[j + 2] = p[-1];
      }
    }
    // padding rows, if any
    if (num_rows != BUFLINES) {
      uint8_t *last_row = dp + static_cast<size_t>(num_rows - 1) * out_width;
      uint8_t *tail     = dp + static_cast<size_t>(num_rows) * out_width;
      for (int i = 0; i < num_extra_rows; ++i) {
        memcpy(tail + static_cast<size_t>(i) * out_width, last_row, out_width);
      }
    }
    return buf.get();
  }

  ~imchunk() = default;
};