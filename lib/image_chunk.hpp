#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#include "constants.hpp"
#include "pnm.hpp"

class imchunk {
 private:
  int width;
  int height;
  int ncomp;
  uint8_t *g_buf;
  uint8_t *buf;
  size_t cur_line;

 public:
  explicit imchunk(const std::string &name) : width(0), height(0), ncomp(1), cur_line(0) {
    g_buf = read_pnm(name, width, height, ncomp);
    buf   = new uint8_t[width * ncomp * LINES];
  }

  [[nodiscard]] int get_width() const { return width; }
  [[nodiscard]] int get_height() const { return height; }
  [[nodiscard]] int get_num_comps() const { return ncomp; }

  uint8_t *get_lines_from(int n) {
    cur_line = n * LINES;
    if (cur_line > height) {
      std::cerr << "ERROR: Exceed height of the image." << std::endl;
      exit(EXIT_FAILURE);
    }
    uint8_t *p = g_buf + width * cur_line * ncomp;
    memcpy(buf, p, width * LINES * ncomp);
    return buf;
  }

  ~imchunk() {
    std::free(g_buf);
    delete[] buf;
  }
};