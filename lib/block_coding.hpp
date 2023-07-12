#pragma once

#include "bitstream.hpp"

void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc);