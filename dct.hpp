#pragma once

#include <cstdint>
#include <vector>

void blkdct2(std::vector<int16_t *> in, int stride, double fx, double fy);