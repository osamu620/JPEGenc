#pragma once

#include <cstdint>
#include <vector>

namespace jpegenc_hwy {
void dct2(std::vector<int16_t *> in, int width, int YCCtype);
}