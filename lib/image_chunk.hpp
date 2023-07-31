#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include <hwy/aligned_allocator.h>

#include "constants.hpp"

class imchunk {
 private:
  int width;
  int height;
  int ncomp;
  uint8_t *g_buf;
  std::unique_ptr<uint8_t[], hwy::AlignedFreer> buf;
  int32_t cur_line;

 public:
  explicit imchunk(uint8_t *imdata, const int w, const int h, const int nc)
      : width(w), height(h), ncomp(nc), g_buf(imdata), cur_line(0) {
    buf = hwy::AllocateAligned<uint8_t>(width * ncomp * LINES);
  }

  uint8_t *get_lines_from(int n) {
    cur_line = n;
    if (cur_line > height) {
      std::cerr << "ERROR: Exceed height of the image." << std::endl;
      exit(EXIT_FAILURE);
    }
    uint8_t *p = g_buf + width * cur_line * ncomp;
    memcpy(buf.get(), p, static_cast<size_t>(width) * LINES * ncomp);
    return buf.get();
  }

  ~imchunk() {
    //    std::free(g_buf);
    //    delete[] buf;
  }
};