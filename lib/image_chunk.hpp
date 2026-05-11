#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include <hwy/aligned_allocator.h>
#include <hwy/highway.h>

#include "constants.hpp"
#include "ycctype.hpp"

class imchunk {
 private:
  const int height;
  const int ncomp;
  const size_t in_width;
  const size_t out_width;
  int origin;
  FILE *file;
  hwy::AlignedFreeUniquePtr<uint8_t[]> buf_tmp;
  int32_t cur_line;
  size_t expected_pos;

 public:
  explicit imchunk(FILE *imdata, const int p, const int w, const int h, const int nc, const int YCCtype)
      : height(h),
        ncomp(nc),
        in_width(w * ncomp),
        out_width(round_up(w, HWY_MAX(DCTSIZE * (YCC_HV[YCCtype][0] >> 4), HWY_MAX_BYTES)) * ncomp),
        origin(p),
        file(imdata),
        buf_tmp(hwy::AllocateAligned<uint8_t>(in_width * BUFLINES)),
        cur_line(0),
        expected_pos(0) {}

  void init() {
    fseek(file, origin, SEEK_SET);
    expected_pos = static_cast<size_t>(origin);
  }

  // Read BUFLINES rows starting at `n` into the caller-supplied buffer `dp`,
  // padding the right edge to `out_width` (replicating the last pixel triple)
  // and the bottom to a full strip if the image ends mid-strip.
  void get_lines_from(int n, uint8_t *dp) {
    cur_line                 = n;
    const int num_rows       = ((cur_line + BUFLINES) > height) ? height % BUFLINES : BUFLINES;
    const int num_extra_rows = ((cur_line + BUFLINES) > height) ? BUFLINES - height % BUFLINES : 0;
    const size_t want_pos    = static_cast<size_t>(origin) + static_cast<size_t>(n) * in_width;
    // Skip the seek when sequential reads keep us at the right offset already.
    // fseek invalidates the FILE's internal buffer, so it isn't free.
    if (want_pos != expected_pos) {
      fseek(file, want_pos, SEEK_SET);
    }
    const size_t bytes_read = in_width * num_rows;
    if (in_width == out_width) {
      fread(dp, sizeof(unsigned char), bytes_read, file);
    } else {
      uint8_t *sp = buf_tmp.get();
      fread(sp, sizeof(unsigned char), bytes_read, file);
      const size_t extra_cols = out_width - in_width;
      for (int i = 0; i < num_rows; ++i) {
        uint8_t *dst = dp + i * out_width;
        memcpy(dst, sp + i * in_width, in_width);
        uint8_t *p = dst + in_width;
        for (size_t j = 0; j < extra_cols; j += ncomp) {
          p[j]     = p[-3];
          p[j + 1] = p[-2];
          p[j + 2] = p[-1];
        }
      }
    }
    expected_pos = want_pos + bytes_read;
    if (num_rows != BUFLINES) {
      uint8_t *src = dp + static_cast<size_t>(num_rows - 1) * out_width;
      uint8_t *fill = dp + static_cast<size_t>(num_rows) * out_width;
      for (int i = 0; i < num_extra_rows; ++i) {
        memcpy(fill, src, out_width);
        fill += out_width;
      }
    }
  }

  ~imchunk() = default;
};