#pragma once
#include <cmath>
#include "bitstream.hpp"
void create_mainheader(int width, int height, int QF, int YCCtype, bitstream &enc, bool use_RESET = false);
void create_scaled_qtable(int c, int QF, int16_t *qtable);