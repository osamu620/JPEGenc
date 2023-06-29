#pragma once
#include <cstdint>
#include <vector>

void rgb2ycbcr(int16_t *in, int width);
void subsample(int16_t *in, std::vector<int16_t *> out, int width, int YCCtype);