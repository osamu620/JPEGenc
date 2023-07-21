#pragma once
#include <cstdint>
#include <vector>

void rgb2ycbcr(uint8_t *in, int width);
void subsample(uint8_t *in, std::vector<int16_t *> out, int width, int YCCtype);

namespace jpegenc_hwy {
void rgb2ycbcr(uint8_t *in, int width);
void subsample(uint8_t *in, std::vector<int16_t *> out, int width, int YCCtype);
}  // namespace jpegenc_hwy