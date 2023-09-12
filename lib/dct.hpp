#pragma once

#include <cstdint>
#include <vector>

namespace jpegenc_hwy {
HWY_ATTR void dct2_core(int16_t *HWY_RESTRICT data);
void dct2(std::vector<int16_t *> in, int width, int mcu_height, int YCCtype);
}  // namespace jpegenc_hwy