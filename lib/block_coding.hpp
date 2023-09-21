#pragma once

#include "bitstream.hpp"

#include <hwy/aligned_allocator.h>

namespace jpegenc_hwy {

struct huff_info {
  std::unique_ptr<uint8_t[], hwy::AlignedFreer> DC_;
  std::unique_ptr<uint8_t[], hwy::AlignedFreer> AC_;
  uint16_t *DC_cwd;
  uint16_t *AC_cwd;
  uint8_t *DC_len;
  uint8_t *AC_len;
  huff_info(const uint16_t *dc, const uint16_t *ac, const uint8_t *dl, const uint8_t *al) {
    DC_    = hwy::AllocateAligned<uint8_t>(static_cast<size_t>(16 + 32));
    AC_    = hwy::AllocateAligned<uint8_t>(static_cast<size_t>(256 + 512));
    DC_cwd = (uint16_t *)DC_.get();
    AC_cwd = (uint16_t *)AC_.get();
    DC_len = DC_.get() + 32;
    AC_len = AC_.get() + 512;
    memcpy(DC_cwd, dc, sizeof(uint16_t) * 16);
    memcpy(AC_cwd, ac, sizeof(uint16_t) * 256);
    memcpy(DC_len, dl, 16);
    memcpy(AC_len, al, 256);
  }
};

void encode_lines(std::vector<int16_t *> &in, int16_t *HWY_RESTRICT mcu, int width, int mcu_height,
                  int YCCtype, int *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &table_Y,
                  huff_info &table_C, bitstream &enc);
}  // namespace jpegenc_hwy