#pragma once
#include <cstdint>
#include <vector>

namespace jpegenc_hwy {
void rgb2ycbcr(uint8_t *in, std::vector<uint8_t *> &out, int width);
void subsample(std::vector<uint8_t *> &in, std::vector<int16_t *> &out, int width, int YCCtype);
}  // namespace jpegenc_hwy