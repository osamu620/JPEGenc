#include "quantization.hpp"

#include <cmath>

#include "constants.hpp"
void create_qtable(int c, int QF, int *qtable) {
  float scale = (QF < 50) ? 5000 / QF : 200 - 2 * QF;
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0) / 100.0;
    stepsize = floor(stepsize);
    if (stepsize < 1.0) {
      stepsize = 1;
    }
    if (stepsize > 255) {
      stepsize = 255;
    }
    qtable[i] = stepsize;
  }
}

void create_qtable2(int c, int QF, int *qtable) {
  float scale = (QF < 50) ? 5000 / QF : 200 - 2 * QF;
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0) / 100.0;
    int val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0) {
      stepsize = 1;
    }
    if (stepsize > 255) {
      stepsize = 255;
    }
    val = (1.0 / stepsize) * (1 << 15);
    // val = (val > 0x7FFF) ? 0x7FFF : val;
    qtable[i] = val;
  }
}

void quantize_fwd(int16_t *in, int *qtable, int stride) {
  int shift = 15 + FRACBITS - 8;
  int half = 1 << (shift - 1);
  for (int i = 0; i < 8; ++i) {
    int16_t *sp = in + i * stride;
    for (int j = 0; j < 8; ++j) {
      sp[j] = (sp[j] * qtable[i * 8 + j] + half) >> shift;
      // if (sp[j] > 2047) {
      //   sp[j] = 2047;
      // }
      // if (sp[j] < -2047) {
      //   sp[j] = -2047;
      // }
      // in[i * stride + j] = (in[i * stride + j] * qtable[i * 8 + j]) >> (15 + (FRACBITS - 8));
    }
  }
}
void blkquantize(std::vector<int16_t *> in, int *qtableL, int *qtableC, int width, double fx, double fy) {
  int scale_x = 1.0 / fx;
  int scale_y = 1.0 / fy;
  int nc = in.size();

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