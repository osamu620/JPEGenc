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
  hwy::AlignedFreeUniquePtr<uint8_t[]> buf_tmp;
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
        buf_tmp(hwy::AllocateAligned<uint8_t>(in_width * BUFLINES)),
        buf(hwy::AllocateAligned<uint8_t>(out_width * BUFLINES)),
        cur_line(0) {}

  void init() { fseek(file, origin, SEEK_SET); }
  uint8_t *get_lines_from(int n) {
    cur_line                 = n;
    const int num_rows       = ((cur_line + BUFLINES) > height) ? height % BUFLINES : BUFLINES;
    const int num_extra_rows = ((cur_line + BUFLINES) > height) ? BUFLINES - height % BUFLINES : 0;
    uint8_t *sp              = buf_tmp.get();
    uint8_t *dp              = buf.get();
    fread(sp, sizeof(unsigned char), in_width * num_rows, file);
    uint8_t *spp[BUFLINES], *dpp[BUFLINES];
    for (int i = 0; i < BUFLINES; ++i) {
      spp[i] = sp + i * in_width;
      dpp[i] = dp + i * out_width;
    }
    const size_t extra_cols = out_width - in_width;
    for (int i = 0; i < num_rows; ++i) {
      memcpy(dpp[i], spp[i], in_width);
      // padding single row
      for (size_t j = 0; j < extra_cols; j += ncomp) {
        uint8_t *p = dpp[i] + in_width;
        p[j]       = p[-3];
        p[j + 1]   = p[-2];
        p[j + 2]   = p[-1];
      }
    }
    // padding rows, if any
    if (num_rows != BUFLINES) {
      sp = dpp[num_rows - 1];
      dp = dpp[num_rows];
      for (int i = 0; i < num_extra_rows; ++i) {
        memcpy(dp, sp, out_width);
        dp += out_width;
      }
    }
    return buf.get();
  }

  ~imchunk() = default;
};