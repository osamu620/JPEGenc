#pragma once

#include "bitstream.hpp"
void create_mainheader(int width, int height, int nc, int *qtable_L, int *qtable_C, int YCCtype,
                       bitstream &enc);