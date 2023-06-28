#include <cmath>
#include "ycctype.hpp"
#include "constants.hpp"
#include "quantization.hpp"

void create_qtable(int c, int QF, int *qtable) {
  float scale = (QF < 50) ? 5000.0F / QF : 200.0F - 2.0F * QF;
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
    int val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0) {
      stepsize = 1;
    }
    if (stepsize > 255) {
      stepsize = 255;
    }
    val       = static_cast<int>((1.0F / stepsize) * (1 << 15));
    qtable[i] = val;
  }
}

static inline void quantize_fwd(int16_t *in, const int *qtable, int stride) {
  int shift = 15 + FRACBITS - 8;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < 8; ++i) {
    int16_t *sp = in + i * stride;
    for (int j = 0; j < 8; ++j) {
      sp[j] = static_cast<int16_t>((sp[j] * qtable[i * 8 + j] + half) >> shift);
    }
  }
}

void quantize(std::vector<int16_t *> in, int *qtableL, int *qtableC, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = in.size();

  int stride = width;
  for (int y = 0; y < LINES; y += 8) {
    int16_t *sp = in[0] + stride * y;
    for (int x = 0; x < stride; x += 8) {
      quantize_fwd(sp + x, qtableL, stride);
    }
  }
  stride = width / scale_x;
  for (int c = 1; c < nc; ++c) {
    for (int y = 0; y < LINES / scale_y; y += 8) {
      int16_t *sp = in[c] + stride * y;
      for (int x = 0; x < stride; x += 8) {
        quantize_fwd(sp + x, qtableC, stride);
      }
    }
  }
}