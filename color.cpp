#include "color.hpp"
#include "ycctype.hpp"
#include "constants.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
#endif

void rgb2ycbcr(int16_t *in, int width) {
#if not defined(JPEG_USE_NEON)
  int16_t *I0 = in, *I1 = in + 1, *I2 = in + 2;
  constexpr int32_t c00   = 9798;    // 0.299 * 2^15
  constexpr int32_t c01   = 19235;   // 0.587 * 2^15
  constexpr int32_t c02   = 3736;    // 0.114 * 2^15
  constexpr int32_t c10   = -5528;   // -0.1687 * 2^15
  constexpr int32_t c11   = -10856;  // -0.3313 * 2^15
  constexpr int32_t c12   = 16384;   // 0.5 * 2^15
  constexpr int32_t c20   = 16384;   // 0.5 * 2^15
  constexpr int32_t c21   = -13720;  // -0.4187 * 2^15
  constexpr int32_t c22   = -2664;   // -0.0813 * 2^15
  constexpr int32_t shift = 15;
  constexpr int32_t half  = 1 << (shift - 1);
  int32_t Y, Cb, Cr;
  for (int i = 0; i < width * 3 * LINES; i += 3) {
    Y     = ((c00 * I0[0] + c01 * I1[0] + c02 * I2[0] + half) >> shift);
    Cb    = (c10 * I0[0] + c11 * I1[0] + c12 * I2[0] + half) >> shift;
    Cr    = (c20 * I0[0] + c21 * I1[0] + c22 * I2[0] + half) >> shift;
    I0[0] = static_cast<int16_t>(Y);
    I1[0] = static_cast<int16_t>(Cb);
    I2[0] = static_cast<int16_t>(Cr);
    I0 += 3;
    I1 += 3;
    I2 += 3;
  }
#else
  const int16x8_t inv_CB_FACT_B = vdupq_n_s16(18492);
  const int16x8_t inv_CR_FACT_R = vdupq_n_s16(23372);
  const int16x8_t c00           = vdupq_n_s16(9798);
  const int16x8_t c01           = vdupq_n_s16(19235);
  const int16x8_t c02           = vdupq_n_s16(3736);
  //  const int16x8_t c10           = vdupq_n_s16(-5528);
  //  const int16x8_t c11           = vdupq_n_s16(-10856);
  //  const int16x8_t c12           = vdupq_n_s16(16384);
  //  const int16x8_t c20           = vdupq_n_s16(16384);
  //  const int16x8_t c21           = vdupq_n_s16(-13720);
  //  const int16x8_t c22           = vdupq_n_s16(-2664);
  int16x8x3_t v;
  int16x8_t R, G, B;
  int16x8_t Y, Cb, Cr;
  for (int i = 0; i < width * 3 * LINES; i += 3 * 8) {
    v        = vld3q_s16(in + i);
    R        = v.val[0];
    G        = v.val[1];
    B        = v.val[2];
    Y        = vaddq_s16(vaddq_s16(vqrdmulhq_s16(R, c00), vqrdmulhq_s16(G, c01)), vqrdmulhq_s16(B, c02));
    Cb       = vqrdmulhq_s16(vsubq_s16(B, Y), inv_CB_FACT_B);
    Cr       = vqrdmulhq_s16(vsubq_s16(R, Y), inv_CR_FACT_R);
    v.val[0] = Y;
    v.val[1] = Cb;
    v.val[2] = Cr;
    vst3q_s16(in + i, v);
  }
#endif
}

void subsample(int16_t *in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc      = out.size();
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;

  int shift = 0;
  int bound = 1;
  while ((scale_x * scale_y) > bound) {
    bound += bound;
    shift++;
  }
  int half = 0;
  if (shift) {
    half = 1 << (shift - 1);
  }
  // Luma, Y, just copying
  size_t pos = 0;
  for (int i = 0; i < LINES; i += DCTSIZE) {
    for (int j = 0; j < width; j += DCTSIZE) {
      auto sp = in + nc * i * width + nc * j;
      for (int y = 0; y < DCTSIZE; ++y) {
        for (int x = 0; x < DCTSIZE; ++x) {
          out[0][pos] = sp[y * width * nc + nc * x];
          pos++;
        }
      }
    }
  }
  // Chroma, Cb and Cr
  for (int c = 1; c < nc; ++c) {
    pos = 0;
    for (int i = 0; i < LINES; i += DCTSIZE * scale_y) {
      for (int j = 0; j < width; j += DCTSIZE * scale_x) {
        auto sp = in + nc * i * width + nc * j + c;
        for (int y = 0; y < DCTSIZE * scale_y; y += scale_y) {
          for (int x = 0; x < DCTSIZE * scale_x; x += scale_x) {
            int ave = 0;
            for (int p = 0; p < scale_y; ++p) {
              for (int q = 0; q < scale_x; ++q) {
                ave += sp[(y + p) * width * nc + nc * (x + q)];
              }
            }
            ave += half;
            ave >>= shift;
            out[c][pos] = static_cast<int16_t>(ave);
            pos++;
          }
        }
      }
    }
  }
}