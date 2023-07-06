#include <cmath>
#include "ycctype.hpp"
#include "constants.hpp"
#include "quantization.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
constexpr float qscale[64] = {
    0.1250, 0.0901, 0.0957, 0.1063, 0.1250, 0.1591, 0.2310, 0.4531, 0.0901, 0.0650, 0.0690, 0.0766, 0.0901,
    0.1147, 0.1665, 0.3266, 0.0957, 0.0690, 0.0732, 0.0814, 0.0957, 0.1218, 0.1768, 0.3468, 0.1063, 0.0766,
    0.0814, 0.0904, 0.1063, 0.1353, 0.1964, 0.3853, 0.1250, 0.0901, 0.0957, 0.1063, 0.1250, 0.1591, 0.2310,
    0.4531, 0.1591, 0.1147, 0.1218, 0.1353, 0.1591, 0.2025, 0.2940, 0.5766, 0.2310, 0.1665, 0.1768, 0.1964,
    0.2310, 0.2940, 0.4268, 0.8372, 0.4531, 0.3266, 0.3468, 0.3853, 0.4531, 0.5766, 0.8372, 1.6421};
#endif

void create_qtable(int c, int QF, int16_t *qtable) {
  float scale = (QF < 50) ? 5000.0F / QF : 200.0F - 2.0F * QF;
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
    int val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0F) {
      stepsize = 1.0F;
    }
    if (stepsize > 255.0F) {
      stepsize = 255.0F;
    }
#if not defined(JPEG_USE_NEON)
    val = static_cast<int>((1.0F / stepsize) * (1 << 15) + 0.5);
#else
    val = static_cast<int>((qscale[i] / stepsize) * (1 << 15) + 0.5);
#endif
    val       = (val > 0x7FFFU) ? 0x7FFF : val;
    qtable[i] = val;
  }
}

static inline void quantize_fwd(int16_t *in, const int16_t *qtable, int stride) {
#if not defined(JPEG_USE_NEON)
  int shift = 15 + FRACBITS - 8;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < DCTSIZE * DCTSIZE; ++i) {
    in[i] = static_cast<int16_t>((in[i] * qtable[i] + half) >> shift);
  }
#else
  int16x8_t q0 = vld1q_s16(qtable);
  int16x8_t q1 = vld1q_s16(qtable + DCTSIZE);
  int16x8_t q2 = vld1q_s16(qtable + DCTSIZE * 2);
  int16x8_t q3 = vld1q_s16(qtable + DCTSIZE * 3);
  int16x8_t q4 = vld1q_s16(qtable + DCTSIZE * 4);
  int16x8_t q5 = vld1q_s16(qtable + DCTSIZE * 5);
  int16x8_t q6 = vld1q_s16(qtable + DCTSIZE * 6);
  int16x8_t q7 = vld1q_s16(qtable + DCTSIZE * 7);

  vst1q_s16(in, vqrdmulhq_s16(vld1q_s16(in), q0));
  vst1q_s16(in + DCTSIZE, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE), q1));
  vst1q_s16(in + DCTSIZE * 2, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 2), q2));
  vst1q_s16(in + DCTSIZE * 3, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 3), q3));
  vst1q_s16(in + DCTSIZE * 4, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 4), q4));
  vst1q_s16(in + DCTSIZE * 5, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 5), q5));
  vst1q_s16(in + DCTSIZE * 6, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 6), q6));
  vst1q_s16(in + DCTSIZE * 7, vqrdmulhq_s16(vld1q_s16(in + DCTSIZE * 7), q7));
#endif
}

void quantize(std::vector<int16_t *> in, int16_t *qtableL, int16_t *qtableC, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = in.size();

  for (int i = 0; i < width * LINES; i += DCTSIZE * DCTSIZE) {
    quantize_fwd(in[0] + i, qtableL, 0);
  }
  for (int c = 1; c < nc; ++c) {
    for (int i = 0; i < width / scale_x * LINES / scale_y; i += DCTSIZE * DCTSIZE) {
      quantize_fwd(in[c] + i, qtableC, 0);
    }
  }
}