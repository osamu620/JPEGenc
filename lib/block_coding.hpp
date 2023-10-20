#pragma once

#include "bitstream.hpp"
#include "huffman_tables.hpp"
#include <cstring>
#include <hwy/aligned_allocator.h>

namespace jpegenc_hwy {

struct huff_info {
  HWY_ALIGN uint8_t DC_[16 + 32];
  HWY_ALIGN uint8_t AC_[256 + 512];
  uint16_t *DC_cwd;
  uint16_t *AC_cwd;
  uint8_t *DC_len;
  uint8_t *AC_len;

  huff_info() : DC_cwd(nullptr), AC_cwd(nullptr), DC_len(nullptr), AC_len(nullptr){};

  template <int C>
  void init(const uint16_t *dc = &DC_cwd_[C][0], const uint16_t *ac = &AC_cwd_[C][0],
            const uint8_t *dl = &DC_len_[C][0], const uint8_t *al = &AC_len_[C][0]) {
    DC_cwd = (uint16_t *)&DC_[0];
    AC_cwd = (uint16_t *)&AC_[0];
    DC_len = &DC_[32];
    AC_len = &AC_[512];
    memcpy(DC_cwd, dc, sizeof(uint16_t) * 16);
    memcpy(AC_cwd, ac, sizeof(uint16_t) * 256);
    memcpy(DC_len, dl, 16);
    memcpy(AC_len, al, 256);
  }
};

void encode_lines(std::vector<int16_t *> &in, int width, int mcu_height, int YCCtype,
                  int16_t *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &table_Y,
                  huff_info &table_C, bitstream &enc);
}  // namespace jpegenc_hwy