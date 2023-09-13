#pragma once

#include "bitstream.hpp"

namespace jpegenc_hwy {
struct huff_info {
  const uint16_t *HWY_RESTRICT DC_cwd;
  const uint16_t *HWY_RESTRICT AC_cwd;
  const uint8_t *HWY_RESTRICT DC_len;
  const uint8_t *HWY_RESTRICT AC_len;
  huff_info(const uint16_t *HWY_RESTRICT dc, const uint16_t *HWY_RESTRICT ac,
            const uint8_t *HWY_RESTRICT dl, const uint8_t *HWY_RESTRICT al)
      : DC_cwd(dc), AC_cwd(ac), DC_len(dl), AC_len(al) {}
};

void Encode_MCUs(std::vector<int16_t *> &in, int width, int mcu_height, int YCCtype,
                 std::vector<int> &prev_dc, huff_info &table_Y, huff_info &table_C, bitstream &enc);
}  // namespace jpegenc_hwy