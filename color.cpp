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
  // Version of 32-bit for internal precision
  //  const int32x4_t inv_CB_FACT_B = vdupq_n_s32(36984);  // 18492
  //  const int32x4_t inv_CR_FACT_R = vdupq_n_s32(46745);  // 23372
  //  const int32x4_t c00           = vdupq_n_s32(19595);  // 9798
  //  const int32x4_t c01           = vdupq_n_s32(38470);  // 19235
  //  const int32x4_t c02           = vdupq_n_s32(7471);   // 3736
  //  int16x8x3_t v;
  //  int32x4_t R0, R1, G0, G1, B0, B1;
  //  int32x4_t Y0, Y1, Cb0, Cb1, Cr0, Cr1;
  //  for (int i = 0; i < width * 3 * LINES; i += 3 * 8) {
  //    v  = vld3q_s16(in + i);
  //    R0 = vmovl_s16(vget_low_s16(v.val[0]));
  //    R1 = vmovl_high_s16(v.val[0]);
  //    G0 = vmovl_s16(vget_low_s16(v.val[1]));
  //    G1 = vmovl_high_s16(v.val[1]);
  //    B0 = vmovl_s16(vget_low_s16(v.val[2]));
  //    B1 = vmovl_high_s16(v.val[2]);
  //
  //    Y0 = vaddq_s32(vaddq_s32(vmulq_s32(R0, c00), vmulq_s32(G0, c01)), vmulq_s32(B0, c02));
  //    Y1 = vaddq_s32(vaddq_s32(vmulq_s32(R1, c00), vmulq_s32(G1, c01)), vmulq_s32(B1, c02));
  //    Y0 = vrshrq_n_s32(Y0, 16);
  //    Y1 = vrshrq_n_s32(Y1, 16);
  //
  //    Cb0 = vmulq_s32(vsubq_s32(B0, Y0), inv_CB_FACT_B);
  //    Cr0 = vmulq_s32(vsubq_s32(R0, Y0), inv_CR_FACT_R);
  //    Cb1 = vmulq_s32(vsubq_s32(B1, Y1), inv_CB_FACT_B);
  //    Cr1 = vmulq_s32(vsubq_s32(R1, Y1), inv_CR_FACT_R);
  //
  //    Cb0 = vrshrq_n_s32(Cb0, 16);
  //    Cb1 = vrshrq_n_s32(Cb1, 16);
  //    Cr0 = vrshrq_n_s32(Cr0, 16);
  //    Cr1 = vrshrq_n_s32(Cr1, 16);
  //
  //    v.val[0] = vcombine_s16(vmovn_s32(Y0), vmovn_s32(Y1));
  //    v.val[1] = vcombine_s16(vmovn_s32(Cb0), vmovn_s32(Cb1));
  //    v.val[2] = vcombine_s16(vmovn_s32(Cr0), vmovn_s32(Cr1));
  //    vst3q_s16(in + i, v);
  //}
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
  size_t pos        = 0;
  size_t pos_Chroma = 0;
#if not defined(JPEG_USE_NEON)
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
#else
  switch (YCCtype) {
    case YCC::YUV444:
      pos = 0;
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            int16x8x3_t v = vld3q_s16(sp + y * width * nc);
            vst1q_s16(out[0] + pos, v.val[0]);
            vst1q_s16(out[1] + pos, v.val[1]);
            vst1q_s16(out[2] + pos, v.val[2]);
            pos += 8;
          }
        }
      }
      break;
    case YCC::YUV422:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            int16x8x3_t v = vld3q_s16(sp + y * width * nc);
            vst1q_s16(out[0] + pos, v.val[0]);
            int16x4_t cb0   = vget_low_s16(v.val[1]);
            int16x4_t cb1   = vget_high_s16(v.val[1]);
            int16x4_t cr0   = vget_low_s16(v.val[2]);
            int16x4_t cr1   = vget_high_s16(v.val[2]);
            int16x4_t cbout = vrshr_n_s16(vpadd_s16(cb0, cb1), 1);
            int16x4_t crout = vrshr_n_s16(vpadd_s16(cr0, cr1), 1);
            // (pos_Chomra % 8) + (pos / 128) * 64 + y * 8
            const size_t pos_C = (pos_Chroma & 0x7) + (pos >> 7) * DCTSIZE * DCTSIZE + y * DCTSIZE;
            vst1_s16(out[1] + pos_C, cbout);
            vst1_s16(out[2] + pos_C, crout);
            pos += 8;
          }
          pos_Chroma += 4;
        }
      }
      break;
    case YCC::YUV440:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          int16x8_t cb, cr;
          pos_Chroma = j * DCTSIZE;
          for (int y = 0; y < DCTSIZE; ++y) {
            int16x8x3_t v = vld3q_s16(sp + y * width * nc);
            vst1q_s16(out[0] + pos, v.val[0]);
            if (y % 2 == 0) {
              cb = v.val[1];
              cr = v.val[2];
            } else {
              // (i / DCTSIZE) * 32
              vst1q_s16(out[1] + (i << 2) + pos_Chroma, vrhaddq_s16(cb, v.val[1]));
              vst1q_s16(out[2] + (i << 2) + pos_Chroma, vrhaddq_s16(cr, v.val[2]));
              pos_Chroma += 8;
            }
            pos += 8;
          }
        }
      }
      break;
    case YCC::YUV420:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          int16x8_t cb, cr;
          int32x4_t t00, t01, t10, t11;
          int32x4_t cb0, cr0;
          // pos_Chroma = (j / 16) * 64 + ((j / 8) % 2) * 4 + (i / DCTSIZE) * 32;
          pos_Chroma = ((j & 0xFFFFFFF0) << 2) + ((j & 0x8) >> 1) + (i << 2);
          for (int y = 0; y < DCTSIZE; ++y) {
            int16x8x3_t v = vld3q_s16(sp + y * width * nc);
            vst1q_s16(out[0] + pos, v.val[0]);
            if (y % 2 == 0) {
              cb = v.val[1];
              cr = v.val[2];
            } else {
              t00 = vaddq_s32(vmovl_s16(vget_low_s16(cb)), vmovl_s16(vget_low_s16(v.val[1])));
              t01 = vaddq_s32(vmovl_high_s16(cb), vmovl_high_s16(v.val[1]));
              cb0 = vrshrq_n_s32(vpaddq_s32(t00, t01), 2);
              t10 = vaddq_s32(vmovl_s16(vget_low_s16(cr)), vmovl_s16(vget_low_s16(v.val[2])));
              t11 = vaddq_s32(vmovl_high_s16(cr), vmovl_high_s16(v.val[2]));
              cr0 = vrshrq_n_s32(vpaddq_s32(t10, t11), 2);
              vst1_s16(out[1] + pos_Chroma, vmovn_s32(cb0));
              vst1_s16(out[2] + pos_Chroma, vmovn_s32(cr0));
              pos_Chroma += 8;
            }
            pos += 8;
          }
        }
      }
      break;

    default:  // For 411, 410
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            int16x8x3_t v = vld3q_s16(sp + y * width * nc);
            vst1q_s16(out[0] + pos, v.val[0]);
            pos += 8;
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
      break;
  }
#endif
}