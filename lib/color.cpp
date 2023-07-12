#include "color.hpp"
#include "ycctype.hpp"
#include "constants.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
  #define FAST_COLOR_CONVERSION
#endif

void rgb2ycbcr(uint8_t *in, int width) {
#if not defined(JPEG_USE_NEON)
  uint8_t *I0 = in, *I1 = in + 1, *I2 = in + 2;
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
    I0[0] = static_cast<uint8_t>(Y);
    I1[0] = static_cast<uint8_t>(Cb + 128);
    I2[0] = static_cast<uint8_t>(Cr + 128);
    I0 += 3;
    I1 += 3;
    I2 += 3;
  }
#else
  #if defined(FAST_COLOR_CONVERSION)
  alignas(16) constexpr uint16_t constants[8] = {19595, 38470 - 32768, 7471,  11056,
                                                 21712, 32768,         27440, 5328};
  const uint16x8_t coeff                      = vld1q_u16(constants);
  const uint16x8_t scaled_128                 = vdupq_n_u16(128 << 1);
  #else
  alignas(16) constexpr uint16_t constants[8] = {19595, 38470, 7471, 11056, 21712, 32768, 27440, 5328};
  const uint16x8_t coeff                      = vld1q_u16(constants);
  const uint32x4_t scaled_128_5               = vdupq_n_u32((128 << 16) + 32767);
  #endif
  uint8x16x3_t v;
  for (size_t i = width * LINES; i > 0; i -= DCTSIZE * 2) {
    v              = vld3q_u8(in);
    uint16x8_t r_l = vmovl_u8(vget_low_u8(v.val[0]));
    uint16x8_t g_l = vmovl_u8(vget_low_u8(v.val[1]));
    uint16x8_t b_l = vmovl_u8(vget_low_u8(v.val[2]));
    uint16x8_t r_h = vmovl_u8(vget_high_u8(v.val[0]));
    uint16x8_t g_h = vmovl_u8(vget_high_u8(v.val[1]));
    uint16x8_t b_h = vmovl_u8(vget_high_u8(v.val[2]));
  #if defined(FAST_COLOR_CONVERSION)
    auto yl        = vreinterpretq_s16_u16(vqrdmulhq_laneq_s16(r_l, coeff, 0));
    yl             = vreinterpretq_s16_u16(vqrdmlahq_laneq_s16(yl, g_l, coeff, 1));
    yl             = vreinterpretq_s16_u16(vqrdmlahq_laneq_s16(yl, b_l, coeff, 2));
    yl             = vrshrq_n_u16(vaddq_u16(yl, g_l), 1);
    auto yh        = vreinterpretq_s16_u16(vqrdmulhq_laneq_s16(r_h, coeff, 0));
    yh             = vreinterpretq_s16_u16(vqrdmlahq_laneq_s16(yh, g_h, coeff, 1));
    yh             = vreinterpretq_s16_u16(vqrdmlahq_laneq_s16(yh, b_h, coeff, 2));
    yh             = vrshrq_n_u16(vaddq_u16(yh, g_h), 1);

    auto cbl = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(scaled_128, r_l, coeff, 3));
    cbl      = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(cbl, g_l, coeff, 4));
    cbl      = vrshrq_n_u16(vaddq_u16(b_l, cbl), 1);
    auto cbh = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(scaled_128, r_h, coeff, 3));
    cbh      = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(cbh, g_h, coeff, 4));
    cbh      = vrshrq_n_u16(vaddq_u16(b_h, cbh), 1);

    auto crl = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(scaled_128, g_l, coeff, 6));
    crl      = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(crl, b_l, coeff, 7));
    crl      = vrshrq_n_u16(vaddq_u16(r_l, crl), 1);
    auto crh = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(scaled_128, g_h, coeff, 6));
    crh      = vreinterpretq_s16_u16(vqrdmlshq_laneq_s16(crh, b_h, coeff, 7));
    crh      = vrshrq_n_u16(vaddq_u16(r_h, crh), 1);

    v.val[0] = vcombine_u8(vmovn_u16(yl), vmovn_u16(yh));
    v.val[1] = vcombine_u8(vmovn_u16(cbl), vmovn_u16(cbh));
    v.val[2] = vcombine_u8(vmovn_u16(crl), vmovn_u16(crh));
    vst3q_u8(in, v);
    in += 3 * DCTSIZE * 2;
  #else
    /* Compute Y = 0.29900 * R + 0.58700 * G + 0.11400 * B */
    uint32x4_t y_ll = vmull_laneq_u16(vget_low_u16(r_l), coeff, 0);
    y_ll            = vmlal_laneq_u16(y_ll, vget_low_u16(g_l), coeff, 1);
    y_ll            = vmlal_laneq_u16(y_ll, vget_low_u16(b_l), coeff, 2);
    uint32x4_t y_lh = vmull_laneq_u16(vget_high_u16(r_l), coeff, 0);
    y_lh            = vmlal_laneq_u16(y_lh, vget_high_u16(g_l), coeff, 1);
    y_lh            = vmlal_laneq_u16(y_lh, vget_high_u16(b_l), coeff, 2);
    uint32x4_t y_hl = vmull_laneq_u16(vget_low_u16(r_h), coeff, 0);
    y_hl            = vmlal_laneq_u16(y_hl, vget_low_u16(g_h), coeff, 1);
    y_hl            = vmlal_laneq_u16(y_hl, vget_low_u16(b_h), coeff, 2);
    uint32x4_t y_hh = vmull_laneq_u16(vget_high_u16(r_h), coeff, 0);
    y_hh            = vmlal_laneq_u16(y_hh, vget_high_u16(g_h), coeff, 1);
    y_hh            = vmlal_laneq_u16(y_hh, vget_high_u16(b_h), coeff, 2);

    /* Compute Cb = -0.16874 * R - 0.33126 * G + 0.50000 * B  + 128 */
    uint32x4_t cb_ll = scaled_128_5;
    cb_ll            = vmlsl_laneq_u16(cb_ll, vget_low_u16(r_l), coeff, 3);
    cb_ll            = vmlsl_laneq_u16(cb_ll, vget_low_u16(g_l), coeff, 4);
    cb_ll            = vmlal_laneq_u16(cb_ll, vget_low_u16(b_l), coeff, 5);
    uint32x4_t cb_lh = scaled_128_5;
    cb_lh            = vmlsl_laneq_u16(cb_lh, vget_high_u16(r_l), coeff, 3);
    cb_lh            = vmlsl_laneq_u16(cb_lh, vget_high_u16(g_l), coeff, 4);
    cb_lh            = vmlal_laneq_u16(cb_lh, vget_high_u16(b_l), coeff, 5);
    uint32x4_t cb_hl = scaled_128_5;
    cb_hl            = vmlsl_laneq_u16(cb_hl, vget_low_u16(r_h), coeff, 3);
    cb_hl            = vmlsl_laneq_u16(cb_hl, vget_low_u16(g_h), coeff, 4);
    cb_hl            = vmlal_laneq_u16(cb_hl, vget_low_u16(b_h), coeff, 5);
    uint32x4_t cb_hh = scaled_128_5;
    cb_hh            = vmlsl_laneq_u16(cb_hh, vget_high_u16(r_h), coeff, 3);
    cb_hh            = vmlsl_laneq_u16(cb_hh, vget_high_u16(g_h), coeff, 4);
    cb_hh            = vmlal_laneq_u16(cb_hh, vget_high_u16(b_h), coeff, 5);

    /* Compute Cr = 0.50000 * R - 0.41869 * G - 0.08131 * B  + 128 */
    uint32x4_t cr_ll = scaled_128_5;
    cr_ll            = vmlal_laneq_u16(cr_ll, vget_low_u16(r_l), coeff, 5);
    cr_ll            = vmlsl_laneq_u16(cr_ll, vget_low_u16(g_l), coeff, 6);
    cr_ll            = vmlsl_laneq_u16(cr_ll, vget_low_u16(b_l), coeff, 7);
    uint32x4_t cr_lh = scaled_128_5;
    cr_lh            = vmlal_laneq_u16(cr_lh, vget_high_u16(r_l), coeff, 5);
    cr_lh            = vmlsl_laneq_u16(cr_lh, vget_high_u16(g_l), coeff, 6);
    cr_lh            = vmlsl_laneq_u16(cr_lh, vget_high_u16(b_l), coeff, 7);
    uint32x4_t cr_hl = scaled_128_5;
    cr_hl            = vmlal_laneq_u16(cr_hl, vget_low_u16(r_h), coeff, 5);
    cr_hl            = vmlsl_laneq_u16(cr_hl, vget_low_u16(g_h), coeff, 6);
    cr_hl            = vmlsl_laneq_u16(cr_hl, vget_low_u16(b_h), coeff, 7);
    uint32x4_t cr_hh = scaled_128_5;
    cr_hh            = vmlal_laneq_u16(cr_hh, vget_high_u16(r_h), coeff, 5);
    cr_hh            = vmlsl_laneq_u16(cr_hh, vget_high_u16(g_h), coeff, 6);
    cr_hh            = vmlsl_laneq_u16(cr_hh, vget_high_u16(b_h), coeff, 7);

    /* Descale Y values (rounding right shift) and narrow to 16-bit. */
    uint16x8_t y_l = vcombine_u16(vrshrn_n_u32(y_ll, 16), vrshrn_n_u32(y_lh, 16));
    uint16x8_t y_h = vcombine_u16(vrshrn_n_u32(y_hl, 16), vrshrn_n_u32(y_hh, 16));
    /* Descale Cb values (right shift) and narrow to 16-bit. */
    uint16x8_t cb_l = vcombine_u16(vshrn_n_u32(cb_ll, 16), vshrn_n_u32(cb_lh, 16));
    uint16x8_t cb_h = vcombine_u16(vshrn_n_u32(cb_hl, 16), vshrn_n_u32(cb_hh, 16));
    /* Descale Cr values (right shift) and narrow to 16-bit. */
    uint16x8_t cr_l = vcombine_u16(vshrn_n_u32(cr_ll, 16), vshrn_n_u32(cr_lh, 16));
    uint16x8_t cr_h = vcombine_u16(vshrn_n_u32(cr_hl, 16), vshrn_n_u32(cr_hh, 16));
    /* Narrow Y, Cb, and Cr values to 8-bit and store to memory.  Buffer
     * overwrite is permitted up to the next multiple of ALIGN_SIZE bytes.
     */
    v.val[0] = vcombine_u8(vmovn_u16(y_l), vmovn_u16(y_h));
    v.val[1] = vcombine_u8(vmovn_u16(cb_l), vmovn_u16(cb_h));
    v.val[2] = vcombine_u8(vmovn_u16(cr_l), vmovn_u16(cr_h));
    vst3q_u8(in, v);
    in += 3 * DCTSIZE * 2;
  #endif
  }
#endif
}

void subsample(uint8_t *in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc      = (YCCtype == YCC::GRAY) ? 1 : 3;
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
  size_t pos = 0;
#if not defined(JPEG_USE_NEON)
  // Luma, Y, just copying
  for (int i = 0; i < LINES; i += DCTSIZE) {
    for (int j = 0; j < width; j += DCTSIZE) {
      auto sp = in + nc * i * width + nc * j;
      for (int y = 0; y < DCTSIZE; ++y) {
        for (int x = 0; x < DCTSIZE; ++x) {
          out[0][pos] = sp[y * width * nc + nc * x] - 128;
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
            out[c][pos] = static_cast<int16_t>(ave - 128);
            pos++;
          }
        }
      }
    }
  }
#else
  size_t pos_Chroma    = 0;
  const uint8x8_t c128 = vdup_n_u8(128);
  switch (YCCtype) {
    case YCC::GRAY:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
          auto sp = in + nc * i * width + nc * j;
          auto v0 = vld1q_u8(sp + 0 * width * nc);
          auto v1 = vld1q_u8(sp + 1 * width * nc);
          auto v2 = vld1q_u8(sp + 2 * width * nc);
          auto v3 = vld1q_u8(sp + 3 * width * nc);
          auto v4 = vld1q_u8(sp + 4 * width * nc);
          auto v5 = vld1q_u8(sp + 5 * width * nc);
          auto v6 = vld1q_u8(sp + 6 * width * nc);
          auto v7 = vld1q_u8(sp + 7 * width * nc);
          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7), c128)));
          pos += 64;
          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7), c128)));
          pos += 64;
        }
      }
      break;

    case YCC::YUV444:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
          auto sp = in + nc * i * width + nc * j;
          auto v0 = vld3q_u8(sp + 0 * width * nc);
          auto v1 = vld3q_u8(sp + 1 * width * nc);
          auto v2 = vld3q_u8(sp + 2 * width * nc);
          auto v3 = vld3q_u8(sp + 3 * width * nc);
          auto v4 = vld3q_u8(sp + 4 * width * nc);
          auto v5 = vld3q_u8(sp + 5 * width * nc);
          auto v6 = vld3q_u8(sp + 6 * width * nc);
          auto v7 = vld3q_u8(sp + 7 * width * nc);

          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[0]), c128)));

          vst1q_s16(out[1] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[1]), c128)));
          vst1q_s16(out[1] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[1]), c128)));

          vst1q_s16(out[2] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[2]), c128)));
          vst1q_s16(out[2] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[2]), c128)));
          pos += 128;
        }
      }
      break;

    case YCC::YUV422:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
          auto sp = in + nc * i * width + nc * j;
          int16x8_t cb00, cb01, cb10, cb11, cb20, cb21, cb30, cb31, cb40, cb41, cb50, cb51, cb60, cb61,
              cb70, cb71;

          auto v0 = vld3q_u8(sp + 0 * width * nc);
          auto v1 = vld3q_u8(sp + 1 * width * nc);
          auto v2 = vld3q_u8(sp + 2 * width * nc);
          auto v3 = vld3q_u8(sp + 3 * width * nc);
          auto v4 = vld3q_u8(sp + 4 * width * nc);
          auto v5 = vld3q_u8(sp + 5 * width * nc);
          auto v6 = vld3q_u8(sp + 6 * width * nc);
          auto v7 = vld3q_u8(sp + 7 * width * nc);

          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[0]), c128)));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[1]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[1]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[1]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[1]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[1]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[1]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[1]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[1]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[1]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[1]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[1]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[1]), c128));

          vst1q_s16(out[1] + pos_Chroma + 8 * 0, vrshrq_n_s16(vpaddq_s16(cb00, cb01), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 1, vrshrq_n_s16(vpaddq_s16(cb10, cb11), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 2, vrshrq_n_s16(vpaddq_s16(cb20, cb21), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 3, vrshrq_n_s16(vpaddq_s16(cb30, cb31), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 4, vrshrq_n_s16(vpaddq_s16(cb40, cb41), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 5, vrshrq_n_s16(vpaddq_s16(cb50, cb51), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 6, vrshrq_n_s16(vpaddq_s16(cb60, cb61), 1));
          vst1q_s16(out[1] + pos_Chroma + 8 * 7, vrshrq_n_s16(vpaddq_s16(cb70, cb71), 1));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[2]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[2]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[2]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[2]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[2]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[2]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[2]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[2]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[2]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[2]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[2]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[2]), c128));

          vst1q_s16(out[2] + pos_Chroma + 8 * 0, vrshrq_n_s16(vpaddq_s16(cb00, cb01), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 1, vrshrq_n_s16(vpaddq_s16(cb10, cb11), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 2, vrshrq_n_s16(vpaddq_s16(cb20, cb21), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 3, vrshrq_n_s16(vpaddq_s16(cb30, cb31), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 4, vrshrq_n_s16(vpaddq_s16(cb40, cb41), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 5, vrshrq_n_s16(vpaddq_s16(cb50, cb51), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 6, vrshrq_n_s16(vpaddq_s16(cb60, cb61), 1));
          vst1q_s16(out[2] + pos_Chroma + 8 * 7, vrshrq_n_s16(vpaddq_s16(cb70, cb71), 1));
          pos += 128;
          pos_Chroma += 64;
        }
      }
      break;

    case YCC::YUV440:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
          auto sp = in + nc * i * width + nc * j;
          int16x8_t cb00, cb01, cb10, cb11, cb20, cb21, cb30, cb31, cb40, cb41, cb50, cb51, cb60, cb61,
              cb70, cb71;
          pos_Chroma = j * 8 + i * 4;
          auto v0    = vld3q_u8(sp + 0 * width * nc);
          auto v1    = vld3q_u8(sp + 1 * width * nc);
          auto v2    = vld3q_u8(sp + 2 * width * nc);
          auto v3    = vld3q_u8(sp + 3 * width * nc);
          auto v4    = vld3q_u8(sp + 4 * width * nc);
          auto v5    = vld3q_u8(sp + 5 * width * nc);
          auto v6    = vld3q_u8(sp + 6 * width * nc);
          auto v7    = vld3q_u8(sp + 7 * width * nc);

          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[0]), c128)));

          vst1q_s16(out[0] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[0]), c128)));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[1]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[1]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[1]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[1]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[1]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[1]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[1]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[1]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[1]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[1]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[1]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[1]), c128));

          vst1q_s16(out[1] + pos_Chroma + 8 * 0, vrhaddq_s16(cb00, cb10));
          vst1q_s16(out[1] + pos_Chroma + 8 * 1, vrhaddq_s16(cb20, cb30));
          vst1q_s16(out[1] + pos_Chroma + 8 * 2, vrhaddq_s16(cb40, cb50));
          vst1q_s16(out[1] + pos_Chroma + 8 * 3, vrhaddq_s16(cb60, cb70));
          vst1q_s16(out[1] + pos_Chroma + 8 * 8, vrhaddq_s16(cb01, cb11));
          vst1q_s16(out[1] + pos_Chroma + 8 * 9, vrhaddq_s16(cb21, cb31));
          vst1q_s16(out[1] + pos_Chroma + 8 * 10, vrhaddq_s16(cb41, cb51));
          vst1q_s16(out[1] + pos_Chroma + 8 * 11, vrhaddq_s16(cb61, cb71));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[2]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[2]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[2]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[2]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[2]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[2]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[2]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[2]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[2]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[2]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[2]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[2]), c128));

          vst1q_s16(out[2] + pos_Chroma + 8 * 0, vrhaddq_s16(cb00, cb10));
          vst1q_s16(out[2] + pos_Chroma + 8 * 1, vrhaddq_s16(cb20, cb30));
          vst1q_s16(out[2] + pos_Chroma + 8 * 2, vrhaddq_s16(cb40, cb50));
          vst1q_s16(out[2] + pos_Chroma + 8 * 3, vrhaddq_s16(cb60, cb70));
          vst1q_s16(out[2] + pos_Chroma + 8 * 8, vrhaddq_s16(cb01, cb11));
          vst1q_s16(out[2] + pos_Chroma + 8 * 9, vrhaddq_s16(cb21, cb31));
          vst1q_s16(out[2] + pos_Chroma + 8 * 10, vrhaddq_s16(cb41, cb51));
          vst1q_s16(out[2] + pos_Chroma + 8 * 11, vrhaddq_s16(cb61, cb71));

          pos += 128;
        }
      }
      break;

    case YCC::YUV420:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
          auto sp = in + nc * i * width + nc * j;
          int16x8_t cb00, cb01, cb10, cb11, cb20, cb21, cb30, cb31, cb40, cb41, cb50, cb51, cb60, cb61,
              cb70, cb71;
          pos_Chroma = j * 4 + i * 4;

          auto v0 = vld3q_u8(sp + 0 * width * nc);
          auto v1 = vld3q_u8(sp + 1 * width * nc);
          auto v2 = vld3q_u8(sp + 2 * width * nc);
          auto v3 = vld3q_u8(sp + 3 * width * nc);
          auto v4 = vld3q_u8(sp + 4 * width * nc);
          auto v5 = vld3q_u8(sp + 5 * width * nc);
          auto v6 = vld3q_u8(sp + 6 * width * nc);
          auto v7 = vld3q_u8(sp + 7 * width * nc);

          vst1q_s16(out[0] + pos + 8 * 0, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 1, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 2, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 3, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 4, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 5, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 6, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 7, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[0]), c128)));

          vst1q_s16(out[0] + pos + 8 * 8, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 9, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 10, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 11, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 12, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 13, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 14, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[0]), c128)));
          vst1q_s16(out[0] + pos + 8 * 15, vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[0]), c128)));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[1]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[1]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[1]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[1]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[1]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[1]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[1]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[1]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[1]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[1]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[1]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[1]), c128));

          vst1q_s16(out[1] + pos_Chroma + 8 * 0,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb00, cb10), vaddq_s16(cb01, cb11)), 2));
          vst1q_s16(out[1] + pos_Chroma + 8 * 1,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb20, cb30), vaddq_s16(cb21, cb31)), 2));
          vst1q_s16(out[1] + pos_Chroma + 8 * 2,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb40, cb50), vaddq_s16(cb41, cb51)), 2));
          vst1q_s16(out[1] + pos_Chroma + 8 * 3,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb60, cb70), vaddq_s16(cb61, cb71)), 2));

          cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128));
          cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128));
          cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128));
          cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128));
          cb20 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v2.val[2]), c128));
          cb21 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v2.val[2]), c128));
          cb30 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v3.val[2]), c128));
          cb31 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v3.val[2]), c128));
          cb40 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v4.val[2]), c128));
          cb41 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v4.val[2]), c128));
          cb50 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v5.val[2]), c128));
          cb51 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v5.val[2]), c128));
          cb60 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v6.val[2]), c128));
          cb61 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v6.val[2]), c128));
          cb70 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v7.val[2]), c128));
          cb71 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v7.val[2]), c128));

          vst1q_s16(out[2] + pos_Chroma + 8 * 0,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb00, cb10), vaddq_s16(cb01, cb11)), 2));
          vst1q_s16(out[2] + pos_Chroma + 8 * 1,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb20, cb30), vaddq_s16(cb21, cb31)), 2));
          vst1q_s16(out[2] + pos_Chroma + 8 * 2,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb40, cb50), vaddq_s16(cb41, cb51)), 2));
          vst1q_s16(out[2] + pos_Chroma + 8 * 3,
                    vrshrq_n_s16(vpaddq_s16(vaddq_s16(cb60, cb70), vaddq_s16(cb61, cb71)), 2));
          pos += 128;
        }
      }
      break;

    case YCC::YUV411:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 4) {
          auto sp  = in + nc * i * width + nc * j;
          size_t p = 0;
          for (int y = 0; y < DCTSIZE; ++y) {
            auto v0 = vld3q_u8(sp + y * width * nc);
            auto v1 = vld3q_u8(sp + y * width * nc + nc * DCTSIZE * 2);
            vst1q_s16(out[0] + pos + p, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 64,
                      vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 128,
                      vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 192,
                      vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
            int16x8_t cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128));
            int16x8_t cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128));
            int16x8_t cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128));
            int16x8_t cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128));
            int16x8_t cr00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128));
            int16x8_t cr01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128));
            int16x8_t cr10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128));
            int16x8_t cr11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128));
            vst1q_s16(out[1] + pos_Chroma + p,
                      vrshrq_n_s16(vpaddq_s16(vpaddq_s16(cb00, cb01), vpaddq_s16(cb10, cb11)), 2));
            vst1q_s16(out[2] + pos_Chroma + p,
                      vrshrq_n_s16(vpaddq_s16(vpaddq_s16(cr00, cr01), vpaddq_s16(cr10, cr11)), 2));
            p += 8;
          }
          pos += 256;
          pos_Chroma += 64;
        }
      }
      break;
    case YCC::YUV410:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 4) {
          auto sp  = in + nc * i * width + nc * j;
          size_t p = 0, pc = 0;
          int16x8_t cb00, cb01, cb10, cb11, cr00, cr01, cr10, cr11;
          int16x8_t cb, cr;
          pos_Chroma = j * 2 + i * 4;
          for (int y = 0; y < DCTSIZE; ++y) {
            auto v0 = vld3q_u8(sp + y * width * nc);
            auto v1 = vld3q_u8(sp + y * width * nc + nc * DCTSIZE * 2);
            vst1q_s16(out[0] + pos + p, vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 64,
                      vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 128,
                      vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[0]), c128)));
            vst1q_s16(out[0] + pos + p + 192,
                      vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[0]), c128)));
            cb00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[1]), c128));
            cb01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[1]), c128));
            cb10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[1]), c128));
            cb11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[1]), c128));
            cr00 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v0.val[2]), c128));
            cr01 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v0.val[2]), c128));
            cr10 = vreinterpretq_s16_u16(vsubl_u8(vget_low_u8(v1.val[2]), c128));
            cr11 = vreinterpretq_s16_u16(vsubl_u8(vget_high_u8(v1.val[2]), c128));
            if (y % 2 == 0) {
              cb = vpaddq_s16(vpaddq_s16(cb00, cb01), vpaddq_s16(cb10, cb11));
              cr = vpaddq_s16(vpaddq_s16(cr00, cr01), vpaddq_s16(cr10, cr11));
            } else {
              cb = vrshrq_n_s16(vaddq_s16(cb, vpaddq_s16(vpaddq_s16(cb00, cb01), vpaddq_s16(cb10, cb11))),
                                3);
              cr = vrshrq_n_s16(vaddq_s16(cr, vpaddq_s16(vpaddq_s16(cr00, cr01), vpaddq_s16(cr10, cr11))),
                                3);
              vst1q_s16(out[1] + pos_Chroma + pc, cb);
              vst1q_s16(out[2] + pos_Chroma + pc, cr);
              pc += 8;
            }
            p += 8;
          }
          pos += 256;
        }
      }
      break;
    case YCC::GRAY2:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            uint8x8x3_t v = vld3_u8(sp + y * width * nc);
            vst1q_s16(out[0] + pos, vreinterpretq_s16_u16(vsubl_u8(v.val[0], c128)));
            pos += 8;
          }
        }
      }
      break;
    default:  // Shall not reach here
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            uint8x8x3_t v = vld3_u8(sp + y * width * nc);
            vst1q_s16(out[0] + pos, vreinterpretq_s16_u16(vsubl_u8(v.val[0], c128)));
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
                out[c][pos] = static_cast<int16_t>(ave - 128);
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