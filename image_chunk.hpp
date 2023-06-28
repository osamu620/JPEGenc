#pragma once

#include <cstdint>
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
  int16_t *buf;
  int cur_line;

 public:
  imchunk(std::string name) : cur_line(0), ncomp(1) {
    g_buf = read_pnm(name, width, height, ncomp);
    buf = new int16_t[width * ncomp * LINES];
  }
  int get_width() { return width; }
  int get_height() { return height; }
  int get_num_comps() { return ncomp; }
  // int16_t *get_buf() { return buf; }
  int16_t *get_lines() {
    uint8_t *p = g_buf + width * cur_line * ncomp;
    for (int i = 0; i < width * LINES * ncomp; ++i) {
      buf[i] = (((int16_t)p[i]) << (FRACBITS - 8)) - (128 << (FRACBITS - 8));
    }
    return buf;
  }
  void advance() {
    cur_line += LINES;
    if (cur_line > height) {
      std::cerr << "Exceed max line" << std::endl;
      exit(EXIT_FAILURE);
    }
  }
  ~imchunk() {
    delete[] g_buf;
    delete[] buf;
  }
};