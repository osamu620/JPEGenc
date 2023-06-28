#pragma once

#include "bitstream.hpp"

void make_zigzag_buffer(std::vector<int16_t *> in, std::vector<int16_t *> out, int width, int YCCtype);
void encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc);