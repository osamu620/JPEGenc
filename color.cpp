#include "color.hpp"
#include "ycctype.hpp"
#include "constants.hpp"

void rgb2ycbcr(int width, int16_t *in) {
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
  for (int i = 0; i < width * 3 * 32; i += 3) {
    Y     = ((c00 * I0[0] + c01 * I1[0] + c02 * I2[0] + half) >> shift);  //- (1 << (FRACBITS - 1));
    Cb    = (c10 * I0[0] + c11 * I1[0] + c12 * I2[0] + half) >> shift;
    Cr    = (c20 * I0[0] + c21 * I1[0] + c22 * I2[0] + half) >> shift;
    I0[0] = static_cast<int16_t>(Y);
    I1[0] = static_cast<int16_t>(Cb);
    I2[0] = static_cast<int16_t>(Cr);
    I0 += 3;
    I1 += 3;
    I2 += 3;
  }
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
  for (int i = 0; i < LINES; ++i) {
    auto sp = in + nc * i * width;
    for (int j = 0; j < width; ++j) {
      out[0][i * width + j] = sp[nc * j];
    }
  }
  // Chroma, Cb and Cr
  const int c_width = width / scale_x;
  for (int c = 1; c < nc; ++c) {
    for (int i = 0, ci = 0; i < LINES; i += scale_y, ++ci) {
      for (int j = 0, cj = 0; j < width; j += scale_x, ++cj) {
        size_t stride = nc * width;
        int16_t *sp   = in + i * stride + nc * j + c;  // top-left
        int ave       = 0;
        for (int y = 0; y < scale_y; ++y) {
          for (int x = 0; x < scale_x; ++x) {
            ave += sp[y * stride + nc * x];
          }
        }
        //        ave /= (scale_y * scale_x);
        ave += half;
        ave >>= shift;
        out[c][ci * c_width + cj] = static_cast<int16_t>(ave);
      }
    }
  }
}