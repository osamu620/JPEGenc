#include "color.hpp"

#include "constants.hpp"

void rgb2ycbcr(int width, int16_t *in) {
  int16_t *R = in, *G = in + 1, *B = in + 2;
  const int32_t c00 = 9798;    // 0.299 * 2^15
  const int32_t c01 = 19235;   // 0.587 * 2^15
  const int32_t c02 = 3736;    // 0.114 * 2^15
  const int32_t c10 = -5528;   // -0.1687 * 2^15
  const int32_t c11 = -10856;  // -0.3313 * 2^15
  const int32_t c12 = 16384;   // 0.5 * 2^15
  const int32_t c20 = 16384;   // 0.5 * 2^15
  const int32_t c21 = -13720;  // -0.4187 * 2^15
  const int32_t c22 = -2664;   // -0.0813 * 2^15
  const int32_t shift = 15;
  const int32_t half = 1 << (shift - 1);
  int16_t Y, Cb, Cr;
  for (int i = 0; i < width * 3 * 32; i += 3) {
    Y = ((c00 * R[0] + c01 * G[0] + c02 * B[0] + half) >> shift);  //- (1 << (FRACBITS - 1));
    Cb = (c10 * R[0] + c11 * G[0] + c12 * B[0] + half) >> shift;
    Cr = (c20 * R[0] + c21 * G[0] + c22 * B[0] + half) >> shift;
    R[0] = Y;
    G[0] = Cb;
    B[0] = Cr;
    R += 3;
    G += 3;
    B += 3;
  }
}

void subsample(int16_t *in, std::vector<int16_t *> out, int in_width, int in_height, double fx, double fy) {
  int nc = out.size();
  int scale_x = 1.0 / fx;
  int scale_y = 1.0 / fy;
  // Luminance, just copy
  for (int i = 0; i < in_height; ++i) {
    auto sp = in + nc * i * in_width;
    for (int j = 0; j < in_width; ++j) {
      out[0][i * in_width + j] = sp[nc * j];
    }
  }
  // Cb
  const int out_width = in_width / scale_x;
  for (int c = 1; c < nc; ++c) {
    for (int i = 0, ci = 0; i < in_height; i += scale_y, ++ci) {
      for (int j = 0, cj = 0; j < in_width; j += scale_x, ++cj) {
        size_t stride = nc * in_width;
        int16_t *sp = in + i * stride + nc * j + c;  // top-left
        int ave = 0;
        for (int y = 0; y < scale_y; ++y) {
          for (int x = 0; x < scale_x; ++x) {
            ave += sp[y * stride + nc * x];
          }
        }
        ave /= (scale_y * scale_x);
        out[c][ci * out_width + cj] = ave;
      }
    }
  }
}