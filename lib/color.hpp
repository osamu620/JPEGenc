#pragma once
#include <cstdint>
#include <vector>

namespace jpegenc_hwy {
void rgb2ycbcr(uint8_t *in, std::vector<uint8_t *> &out, int width);
void subsample(std::vector<uint8_t *> &in, std::vector<int16_t *> &out, int width, int YCCtype);

// Fused RGB → YCbCr → subsample. Single pass over the input for the YCC modes
// that have a fused implementation; falls back to rgb2ycbcr+subsample for any
// mode without one.
void rgb2ycbcr_subsample(uint8_t *in, std::vector<uint8_t *> &yuv0, std::vector<int16_t *> &out, int width,
                         int YCCtype);
}  // namespace jpegenc_hwy