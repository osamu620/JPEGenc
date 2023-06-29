#include "dct.hpp"
#include "constants.hpp"
#include "ycctype.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
#endif

void fastdct2(int16_t *in, int stride) {
  //  static constexpr float S[] = {
  //      0.353553390593273762200422, 0.254897789552079584470970, 0.270598050073098492199862,
  //      0.300672443467522640271861, 0.353553390593273762200422, 0.449988111568207852319255,
  //      0.653281482438188263928322, 1.281457723870753089398043,
  //  };
  static constexpr int32_t scale[]  = {11585, 8352, 8867, 9852, 11585, 14745, 21407, 41991};
  static constexpr int32_t rotate[] = {23170, 12540, 17734, 42813};
  int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int32_t tmp10, tmp11, tmp12, tmp13;
  int32_t z1, z2, z3, z4, z5, z11, z13;
  int16_t *dataptr;
  int ctr;
  constexpr int half = 1 << 14;

  /* Pass 1: process rows. */

  dataptr = in;
  for (ctr = 7; ctr >= 0; ctr--) {
    tmp0 = dataptr[0] + dataptr[7];
    tmp7 = dataptr[0] - dataptr[7];
    tmp1 = dataptr[1] + dataptr[6];
    tmp6 = dataptr[1] - dataptr[6];
    tmp2 = dataptr[2] + dataptr[5];
    tmp5 = dataptr[2] - dataptr[5];
    tmp3 = dataptr[3] + dataptr[4];
    tmp4 = dataptr[3] - dataptr[4];

    /* Even part */

    tmp10 = tmp0 + tmp3; /* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[0] = ((tmp10 + tmp11) * scale[0] + half) >> 15; /* phase 3 */
    dataptr[4] = ((tmp10 - tmp11) * scale[4] + half) >> 15;

    z1         = ((tmp12 + tmp13) * rotate[0] + half) >> 15; /* c4 */
    dataptr[2] = ((tmp13 + z1) * scale[2] + half) >> 15;     /* phase 5 */
    dataptr[6] = ((tmp13 - z1) * scale[6] + half) >> 15;

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((tmp10 - tmp12) * rotate[1] + half) >> 15; /* c6 */
    z2 = ((rotate[2] * tmp10 + half) >> 15) + z5;    /* c2-c6 */
    z4 = ((rotate[3] * tmp12 + half) >> 15) + z5;    /* c2+c6 */
    z3 = (tmp11 * rotate[0] + half) >> 15;           /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[5] = ((z13 + z2) * scale[5] + half) >> 15; /* phase 6 */
    dataptr[3] = ((z13 - z2) * scale[3] + half) >> 15;
    dataptr[1] = ((z11 + z4) * scale[1] + half) >> 15;
    dataptr[7] = ((z11 - z4) * scale[7] + half) >> 15;

    dataptr += stride; /* advance pointer to next row */
  }
  /* Pass 2: process columns. */

  dataptr = in;
#if not defined(JPEG_USE_NEON)
  for (ctr = 8 - 1; ctr >= 0; ctr--) {
    tmp0 = dataptr[stride * 0] + dataptr[stride * 7];
    tmp7 = dataptr[stride * 0] - dataptr[stride * 7];
    tmp1 = dataptr[stride * 1] + dataptr[stride * 6];
    tmp6 = dataptr[stride * 1] - dataptr[stride * 6];
    tmp2 = dataptr[stride * 2] + dataptr[stride * 5];
    tmp5 = dataptr[stride * 2] - dataptr[stride * 5];
    tmp3 = dataptr[stride * 3] + dataptr[stride * 4];
    tmp4 = dataptr[stride * 3] - dataptr[stride * 4];

    /* Even part */

    tmp10 = tmp0 + tmp3; /* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[stride * 0] = ((tmp10 + tmp11) * scale[0] + half) >> 15; /* phase 3 */
    dataptr[stride * 4] = ((tmp10 - tmp11) * scale[4] + half) >> 15;

    z1                  = ((tmp12 + tmp13) * rotate[0] + half) >> 15; /* c4 */
    dataptr[stride * 2] = ((tmp13 + z1) * scale[2] + half) >> 15;     /* phase 5 */
    dataptr[stride * 6] = ((tmp13 - z1) * scale[6] + half) >> 15;

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((tmp10 - tmp12) * rotate[1] + half) >> 15; /* c6 */
    z2 = ((rotate[2] * tmp10 + half) >> 15) + z5;    /* c2-c6 */
    z4 = ((rotate[3] * tmp12 + half) >> 15) + z5;    /* c2+c6 */
    z3 = (tmp11 * rotate[0] + half) >> 15;           /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[stride * 5] = ((z13 + z2) * scale[5] + half) >> 15; /* phase 6 */
    dataptr[stride * 3] = ((z13 - z2) * scale[3] + half) >> 15;
    dataptr[stride * 1] = ((z11 + z4) * scale[1] + half) >> 15;
    dataptr[stride * 7] = ((z11 - z4) * scale[7] + half) >> 15;

    dataptr++; /* advance pointer to next column */
  }
#else
  int32x4_t vtmp0, vtmp1, vtmp2, vtmp3, vtmp4, vtmp5, vtmp6, vtmp7;
  int32x4_t vtmp10, vtmp11, vtmp12, vtmp13;
  int32x4_t vz1, vz2, vz3, vz4, vz5;
  int32x4_t vz11, vz13;
  for (ctr = 8 - 1; ctr >= 0; ctr -= 4) {
    vtmp0 = vaddl_s16(vld1_s16(dataptr + stride * 0), vld1_s16(dataptr + stride * 7));
    vtmp7 = vsubl_s16(vld1_s16(dataptr + stride * 0), vld1_s16(dataptr + stride * 7));
    vtmp1 = vaddl_s16(vld1_s16(dataptr + stride * 1), vld1_s16(dataptr + stride * 6));
    vtmp6 = vsubl_s16(vld1_s16(dataptr + stride * 1), vld1_s16(dataptr + stride * 6));
    vtmp2 = vaddl_s16(vld1_s16(dataptr + stride * 2), vld1_s16(dataptr + stride * 5));
    vtmp5 = vsubl_s16(vld1_s16(dataptr + stride * 2), vld1_s16(dataptr + stride * 5));
    vtmp3 = vaddl_s16(vld1_s16(dataptr + stride * 3), vld1_s16(dataptr + stride * 4));
    vtmp4 = vsubl_s16(vld1_s16(dataptr + stride * 3), vld1_s16(dataptr + stride * 4));

    /* Even part */

    vtmp10 = vtmp0 + vtmp3; /* phase 2 */
    vtmp13 = vtmp0 - vtmp3;
    vtmp11 = vtmp1 + vtmp2;
    vtmp12 = vtmp1 - vtmp2;

    vst1_s16(dataptr + stride * 0, vmovn_s32(((vtmp10 + vtmp11) * scale[0] + half) >> 15)); /* phase 3 */
    vst1_s16(dataptr + stride * 4, vmovn_s32(((vtmp10 - vtmp11) * scale[4] + half) >> 15));

    vz1 = ((vtmp12 + vtmp13) * rotate[0] + half) >> 15;                                  /* c4 */
    vst1_s16(dataptr + stride * 2, vmovn_s32(((vtmp13 + vz1) * scale[2] + half) >> 15)); /* phase 5 */
    vst1_s16(dataptr + stride * 6, vmovn_s32(((vtmp13 - vz1) * scale[6] + half) >> 15));

    /* Odd part */

    vtmp10 = vtmp4 + vtmp5; /* phase 2 */
    vtmp11 = vtmp5 + vtmp6;
    vtmp12 = vtmp6 + vtmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    vz5 = ((vtmp10 - vtmp12) * rotate[1] + half) >> 15; /* c6 */
    vz2 = ((rotate[2] * vtmp10 + half) >> 15) + vz5;    /* c2-c6 */
    vz4 = ((rotate[3] * vtmp12 + half) >> 15) + vz5;    /* c2+c6 */
    vz3 = (vtmp11 * rotate[0] + half) >> 15;            /* c4 */

    vz11 = vtmp7 + vz3; /* phase 5 */
    vz13 = vtmp7 - vz3;

    vst1_s16(dataptr + stride * 5, vmovn_s32(((vz13 + vz2) * scale[5] + half) >> 15)); /* phase 6 */
    vst1_s16(dataptr + stride * 3, vmovn_s32(((vz13 - vz2) * scale[3] + half) >> 15));
    vst1_s16(dataptr + stride * 1, vmovn_s32(((vz11 + vz4) * scale[1] + half) >> 15));
    vst1_s16(dataptr + stride * 7, vmovn_s32(((vz11 - vz4) * scale[7] + half) >> 15));

    dataptr += 4; /* advance pointer to next column */
  }
#endif
}

void dct2(std::vector<int16_t *> in, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = in.size();

  int stride = width;
  for (int y = 0; y < LINES; y += 8) {
    int16_t *sp = in[0] + stride * y;
    for (int x = 0; x < stride; x += 8) {
      fastdct2(sp + x, stride);
    }
  }
  stride = width / scale_x;
  for (int c = 1; c < nc; ++c) {
    for (int y = 0; y < LINES / scale_y; y += 8) {
      int16_t *sp = in[c] + stride * y;
      for (int x = 0; x < stride; x += 8) {
        fastdct2(sp + x, stride);
      }
    }
  }
}