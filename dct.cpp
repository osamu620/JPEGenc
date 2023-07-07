#include "dct.hpp"
#include "constants.hpp"
#include "ycctype.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
  #define F_0_382 12544
  #define F_0_541 17792
  #define F_0_707 23168
  #define F_0_306 9984

static const int16_t jsimd_fdct_ifast_neon_consts[] = {F_0_382, F_0_541, F_0_707, F_0_306};

void fast_dct2_neon(int16_t *data) {
  /* Load an 8x8 block of samples into Neon registers.  De-interleaving loads
   * are used, followed by vuzp to transpose the block such that we have a
   * column of samples per vector - allowing all rows to be processed at once.
   */
  int16x8x4_t data1 = vld4q_s16(data);
  int16x8x4_t data2 = vld4q_s16(data + 4 * DCTSIZE);

  int16x8x2_t cols_04 = vuzpq_s16(data1.val[0], data2.val[0]);
  int16x8x2_t cols_15 = vuzpq_s16(data1.val[1], data2.val[1]);
  int16x8x2_t cols_26 = vuzpq_s16(data1.val[2], data2.val[2]);
  int16x8x2_t cols_37 = vuzpq_s16(data1.val[3], data2.val[3]);

  int16x8_t col0 = cols_04.val[0];
  int16x8_t col1 = cols_15.val[0];
  int16x8_t col2 = cols_26.val[0];
  int16x8_t col3 = cols_37.val[0];
  int16x8_t col4 = cols_04.val[1];
  int16x8_t col5 = cols_15.val[1];
  int16x8_t col6 = cols_26.val[1];
  int16x8_t col7 = cols_37.val[1];

  /* Pass 1: process rows. */

  /* Load DCT conversion constants. */
  const int16x4_t consts = vld1_s16(jsimd_fdct_ifast_neon_consts);

  int16x8_t tmp0 = vaddq_s16(col0, col7);
  int16x8_t tmp7 = vsubq_s16(col0, col7);
  int16x8_t tmp1 = vaddq_s16(col1, col6);
  int16x8_t tmp6 = vsubq_s16(col1, col6);
  int16x8_t tmp2 = vaddq_s16(col2, col5);
  int16x8_t tmp5 = vsubq_s16(col2, col5);
  int16x8_t tmp3 = vaddq_s16(col3, col4);
  int16x8_t tmp4 = vsubq_s16(col3, col4);

  /* Even part */
  int16x8_t tmp10 = vaddq_s16(tmp0, tmp3); /* phase 2 */
  int16x8_t tmp13 = vsubq_s16(tmp0, tmp3);
  int16x8_t tmp11 = vaddq_s16(tmp1, tmp2);
  int16x8_t tmp12 = vsubq_s16(tmp1, tmp2);

  col0 = vaddq_s16(tmp10, tmp11); /* phase 3 */
  col4 = vsubq_s16(tmp10, tmp11);

  int16x8_t z1 = vqdmulhq_lane_s16(vaddq_s16(tmp12, tmp13), consts, 2);
  col2         = vaddq_s16(tmp13, z1); /* phase 5 */
  col6         = vsubq_s16(tmp13, z1);

  /* Odd part */
  tmp10 = vaddq_s16(tmp4, tmp5); /* phase 2 */
  tmp11 = vaddq_s16(tmp5, tmp6);
  tmp12 = vaddq_s16(tmp6, tmp7);

  int16x8_t z5 = vqdmulhq_lane_s16(vsubq_s16(tmp10, tmp12), consts, 0);
  int16x8_t z2 = vqdmulhq_lane_s16(tmp10, consts, 1);
  z2           = vaddq_s16(z2, z5);
  int16x8_t z4 = vqdmulhq_lane_s16(tmp12, consts, 3);
  z5           = vaddq_s16(tmp12, z5);
  z4           = vaddq_s16(z4, z5);
  int16x8_t z3 = vqdmulhq_lane_s16(tmp11, consts, 2);

  int16x8_t z11 = vaddq_s16(tmp7, z3); /* phase 5 */
  int16x8_t z13 = vsubq_s16(tmp7, z3);

  col5 = vaddq_s16(z13, z2); /* phase 6 */
  col3 = vsubq_s16(z13, z2);
  col1 = vaddq_s16(z11, z4);
  col7 = vsubq_s16(z11, z4);

  /* Transpose to work on columns in pass 2. */
  int16x8x2_t cols_01 = vtrnq_s16(col0, col1);
  int16x8x2_t cols_23 = vtrnq_s16(col2, col3);
  int16x8x2_t cols_45 = vtrnq_s16(col4, col5);
  int16x8x2_t cols_67 = vtrnq_s16(col6, col7);

  int32x4x2_t cols_0145_l =
      vtrnq_s32(vreinterpretq_s32_s16(cols_01.val[0]), vreinterpretq_s32_s16(cols_45.val[0]));
  int32x4x2_t cols_0145_h =
      vtrnq_s32(vreinterpretq_s32_s16(cols_01.val[1]), vreinterpretq_s32_s16(cols_45.val[1]));
  int32x4x2_t cols_2367_l =
      vtrnq_s32(vreinterpretq_s32_s16(cols_23.val[0]), vreinterpretq_s32_s16(cols_67.val[0]));
  int32x4x2_t cols_2367_h =
      vtrnq_s32(vreinterpretq_s32_s16(cols_23.val[1]), vreinterpretq_s32_s16(cols_67.val[1]));

  int32x4x2_t rows_04 = vzipq_s32(cols_0145_l.val[0], cols_2367_l.val[0]);
  int32x4x2_t rows_15 = vzipq_s32(cols_0145_h.val[0], cols_2367_h.val[0]);
  int32x4x2_t rows_26 = vzipq_s32(cols_0145_l.val[1], cols_2367_l.val[1]);
  int32x4x2_t rows_37 = vzipq_s32(cols_0145_h.val[1], cols_2367_h.val[1]);

  int16x8_t row0 = vreinterpretq_s16_s32(rows_04.val[0]);
  int16x8_t row1 = vreinterpretq_s16_s32(rows_15.val[0]);
  int16x8_t row2 = vreinterpretq_s16_s32(rows_26.val[0]);
  int16x8_t row3 = vreinterpretq_s16_s32(rows_37.val[0]);
  int16x8_t row4 = vreinterpretq_s16_s32(rows_04.val[1]);
  int16x8_t row5 = vreinterpretq_s16_s32(rows_15.val[1]);
  int16x8_t row6 = vreinterpretq_s16_s32(rows_26.val[1]);
  int16x8_t row7 = vreinterpretq_s16_s32(rows_37.val[1]);

  /* Pass 2: process columns. */

  tmp0 = vaddq_s16(row0, row7);
  tmp7 = vsubq_s16(row0, row7);
  tmp1 = vaddq_s16(row1, row6);
  tmp6 = vsubq_s16(row1, row6);
  tmp2 = vaddq_s16(row2, row5);
  tmp5 = vsubq_s16(row2, row5);
  tmp3 = vaddq_s16(row3, row4);
  tmp4 = vsubq_s16(row3, row4);

  /* Even part */
  tmp10 = vaddq_s16(tmp0, tmp3); /* phase 2 */
  tmp13 = vsubq_s16(tmp0, tmp3);
  tmp11 = vaddq_s16(tmp1, tmp2);
  tmp12 = vsubq_s16(tmp1, tmp2);

  row0 = vaddq_s16(tmp10, tmp11); /* phase 3 */
  row4 = vsubq_s16(tmp10, tmp11);

  z1   = vqdmulhq_lane_s16(vaddq_s16(tmp12, tmp13), consts, 2);
  row2 = vaddq_s16(tmp13, z1); /* phase 5 */
  row6 = vsubq_s16(tmp13, z1);

  /* Odd part */
  tmp10 = vaddq_s16(tmp4, tmp5); /* phase 2 */
  tmp11 = vaddq_s16(tmp5, tmp6);
  tmp12 = vaddq_s16(tmp6, tmp7);

  z5 = vqdmulhq_lane_s16(vsubq_s16(tmp10, tmp12), consts, 0);
  z2 = vqdmulhq_lane_s16(tmp10, consts, 1);
  z2 = vaddq_s16(z2, z5);
  z4 = vqdmulhq_lane_s16(tmp12, consts, 3);
  z5 = vaddq_s16(tmp12, z5);
  z4 = vaddq_s16(z4, z5);
  z3 = vqdmulhq_lane_s16(tmp11, consts, 2);

  z11 = vaddq_s16(tmp7, z3); /* phase 5 */
  z13 = vsubq_s16(tmp7, z3);

  row5 = vaddq_s16(z13, z2); /* phase 6 */
  row3 = vsubq_s16(z13, z2);
  row1 = vaddq_s16(z11, z4);
  row7 = vsubq_s16(z11, z4);

  vst1q_s16(data + 0 * DCTSIZE, row0);
  vst1q_s16(data + 1 * DCTSIZE, row1);
  vst1q_s16(data + 2 * DCTSIZE, row2);
  vst1q_s16(data + 3 * DCTSIZE, row3);
  vst1q_s16(data + 4 * DCTSIZE, row4);
  vst1q_s16(data + 5 * DCTSIZE, row5);
  vst1q_s16(data + 6 * DCTSIZE, row6);
  vst1q_s16(data + 7 * DCTSIZE, row7);
}
#endif
void fastdct2(int16_t *in, int stride) {
  //  static constexpr float S[] = {
  //      0.353553390593273762200422, 0.254897789552079584470970, 0.270598050073098492199862,
  //      0.300672443467522640271861, 0.353553390593273762200422, 0.449988111568207852319255,
  //      0.653281482438188263928322, 1.281457723870753089398043,
  //  };
  static constexpr int32_t scale[]  = {11585, 8352, 8867, 9852, 11585, 14745, 21407, 41991};
  static constexpr int32_t rotate[] = {12540, 17734, 23170, 42813};
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

    z1         = ((tmp12 + tmp13) * rotate[2] + half) >> 15; /* c4 */
    dataptr[2] = ((tmp13 + z1) * scale[2] + half) >> 15;     /* phase 5 */
    dataptr[6] = ((tmp13 - z1) * scale[6] + half) >> 15;

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((tmp10 - tmp12) * rotate[0] + half) >> 15; /* c6 */
    z2 = ((rotate[1] * tmp10 + half) >> 15) + z5;    /* c2-c6 */
    z4 = ((rotate[3] * tmp12 + half) >> 15) + z5;    /* c2+c6 */
    z3 = (tmp11 * rotate[2] + half) >> 15;           /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[5] = ((z13 + z2) * scale[5] + half) >> 15; /* phase 6 */
    dataptr[3] = ((z13 - z2) * scale[3] + half) >> 15;
    dataptr[1] = ((z11 + z4) * scale[1] + half) >> 15;
    dataptr[7] = ((z11 - z4) * scale[7] + half) >> 15;

    dataptr += DCTSIZE; /* advance pointer to next row */
  }
  /* Pass 2: process columns. */

  dataptr = in;

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

    z1                  = ((tmp12 + tmp13) * rotate[2] + half) >> 15; /* c4 */
    dataptr[stride * 2] = ((tmp13 + z1) * scale[2] + half) >> 15;     /* phase 5 */
    dataptr[stride * 6] = ((tmp13 - z1) * scale[6] + half) >> 15;

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((tmp10 - tmp12) * rotate[0] + half) >> 15; /* c6 */
    z2 = ((rotate[1] * tmp10 + half) >> 15) + z5;    /* c2-c6 */
    z4 = ((rotate[3] * tmp12 + half) >> 15) + z5;    /* c2+c6 */
    z3 = (tmp11 * rotate[2] + half) >> 15;           /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[stride * 5] = ((z13 + z2) * scale[5] + half) >> 15; /* phase 6 */
    dataptr[stride * 3] = ((z13 - z2) * scale[3] + half) >> 15;
    dataptr[stride * 1] = ((z11 + z4) * scale[1] + half) >> 15;
    dataptr[stride * 7] = ((z11 - z4) * scale[7] + half) >> 15;

    dataptr++; /* advance pointer to next column */
  }

  //  int32x4_t vtmp0, vtmp1, vtmp2, vtmp3, vtmp4, vtmp5, vtmp6, vtmp7;
  //  int32x4_t vtmp10, vtmp11, vtmp12, vtmp13;
  //  int32x4_t vz1, vz2, vz3, vz4, vz5;
  //  int32x4_t vz11, vz13;
  //  for (ctr = 8 - 1; ctr >= 0; ctr -= 4) {
  //    vtmp0 = vaddl_s16(vld1_s16(dataptr + DCTSIZE * 0), vld1_s16(dataptr + DCTSIZE * 7));
  //    vtmp7 = vsubl_s16(vld1_s16(dataptr + DCTSIZE * 0), vld1_s16(dataptr + DCTSIZE * 7));
  //    vtmp1 = vaddl_s16(vld1_s16(dataptr + DCTSIZE * 1), vld1_s16(dataptr + DCTSIZE * 6));
  //    vtmp6 = vsubl_s16(vld1_s16(dataptr + DCTSIZE * 1), vld1_s16(dataptr + DCTSIZE * 6));
  //    vtmp2 = vaddl_s16(vld1_s16(dataptr + DCTSIZE * 2), vld1_s16(dataptr + DCTSIZE * 5));
  //    vtmp5 = vsubl_s16(vld1_s16(dataptr + DCTSIZE * 2), vld1_s16(dataptr + DCTSIZE * 5));
  //    vtmp3 = vaddl_s16(vld1_s16(dataptr + DCTSIZE * 3), vld1_s16(dataptr + DCTSIZE * 4));
  //    vtmp4 = vsubl_s16(vld1_s16(dataptr + DCTSIZE * 3), vld1_s16(dataptr + DCTSIZE * 4));
  //
  //    /* Even part */
  //
  //    vtmp10 = vtmp0 + vtmp3; /* phase 2 */
  //    vtmp13 = vtmp0 - vtmp3;
  //    vtmp11 = vtmp1 + vtmp2;
  //    vtmp12 = vtmp1 - vtmp2;
  //
  //    vst1_s16(dataptr + DCTSIZE * 0, vmovn_s32(((vtmp10 + vtmp11) * scale[0] + half) >> 15)); /* phase 3
  //    */ vst1_s16(dataptr + DCTSIZE * 4, vmovn_s32(((vtmp10 - vtmp11) * scale[4] + half) >> 15));
  //
  //    vz1 = ((vtmp12 + vtmp13) * rotate[0] + half) >> 15;                                   /* c4 */
  //    vst1_s16(dataptr + DCTSIZE * 2, vmovn_s32(((vtmp13 + vz1) * scale[2] + half) >> 15)); /* phase 5 */
  //    vst1_s16(dataptr + DCTSIZE * 6, vmovn_s32(((vtmp13 - vz1) * scale[6] + half) >> 15));
  //
  //    /* Odd part */
  //
  //    vtmp10 = vtmp4 + vtmp5; /* phase 2 */
  //    vtmp11 = vtmp5 + vtmp6;
  //    vtmp12 = vtmp6 + vtmp7;
  //
  //    /* The rotator is modified from fig 4-8 to avoid extra negations. */
  //    vz5 = ((vtmp10 - vtmp12) * rotate[1] + half) >> 15; /* c6 */
  //    vz2 = ((rotate[2] * vtmp10 + half) >> 15) + vz5;    /* c2-c6 */
  //    vz4 = ((rotate[3] * vtmp12 + half) >> 15) + vz5;    /* c2+c6 */
  //    vz3 = (vtmp11 * rotate[0] + half) >> 15;            /* c4 */
  //
  //    vz11 = vtmp7 + vz3; /* phase 5 */
  //    vz13 = vtmp7 - vz3;
  //
  //    vst1_s16(dataptr + DCTSIZE * 5, vmovn_s32(((vz13 + vz2) * scale[5] + half) >> 15)); /* phase 6 */
  //    vst1_s16(dataptr + DCTSIZE * 3, vmovn_s32(((vz13 - vz2) * scale[3] + half) >> 15));
  //    vst1_s16(dataptr + DCTSIZE * 1, vmovn_s32(((vz11 + vz4) * scale[1] + half) >> 15));
  //    vst1_s16(dataptr + DCTSIZE * 7, vmovn_s32(((vz11 - vz4) * scale[7] + half) >> 15));
  //
  //    dataptr += 4; /* advance pointer to next column */
  //  }
}

void dct2(std::vector<int16_t *> in, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = in.size();

  for (int i = 0; i < width * LINES; i += DCTSIZE * DCTSIZE) {
#if not defined(JPEG_USE_NEON)
    fastdct2(in[0] + i, DCTSIZE);
#else
    fast_dct2_neon(in[0] + i);
#endif
  }
  for (int c = 1; c < nc; ++c) {
    for (int i = 0; i < width / scale_x * LINES / scale_y; i += DCTSIZE * DCTSIZE) {
#if not defined(JPEG_USE_NEON)
      fastdct2(in[c] + i, DCTSIZE);
#else
      fast_dct2_neon(in[c] + i);
#endif
    }
  }
}