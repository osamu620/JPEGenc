#pragma once
#include <cstdint>
#include <vector>

void rgb2ycbcr(int, int16_t *);
void subsample(int16_t *in, std::vector<int16_t *> out, int width, int height, double fx = 1.0,
               double fy = 1.0);