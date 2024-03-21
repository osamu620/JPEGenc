#include <cmath>
#include <hwy/highway.h>

#include "constants.hpp"

namespace hn = hwy::HWY_NAMESPACE;
/* The following DCT algorithm is derived from
 * Yukihiro Arai, Takeshi Agui, and Masayuki Nakajima, "A Fast DCT-SQ Scheme for Images, "
 * IEICE Transactions on Fundamentals of Electronics, Communications and Computer Sciences 71 (1988),
   1095--1097.
 */

// coeffs in floating point  = {0.382683432, 0.541196100, 0.707106718, 1.306562963 - 1.0} * 2^15
//   - four extra zero elements are for 128 bit load op.
static const double c6 = cos((3.0 * M_PI) / 8.0);
static const double c2 = cos((1.0 * M_PI) / 8.0);
static const double c4 = cos((2.0 * M_PI) / 8.0);

static const int16_t a0 = static_cast<int16_t>(round(c6 * (1 << 15)));
static const int16_t a1 = static_cast<int16_t>(round((c2 - c6) * (1 << 15)));
static const int16_t a2 = static_cast<int16_t>(round(c4 * (1 << 15)));
static const int16_t a3 = static_cast<int16_t>(round((c2 + c6 - 1.0) * (1 << 15)));

HWY_ALIGN static const int16_t coeff[] = {a0, a1, a2, a3, 0, 0, 0, 0};

void dct2_core(int16_t *HWY_RESTRICT data) {
#if HWY_TARGET != HWY_SCALAR
  HWY_CAPPED(int16_t, 8) s16;
  //  HWY_CAPPED(int32_t, 4) s32;
  const auto vcoeffs = hn::LoadDup128(s16, coeff);
  auto data1_0       = hn::Undefined(s16);
  auto data1_1       = hn::Undefined(s16);
  auto data1_2       = hn::Undefined(s16);
  auto data1_3       = hn::Undefined(s16);
  auto data2_0       = hn::Undefined(s16);
  auto data2_1       = hn::Undefined(s16);
  auto data2_2       = hn::Undefined(s16);
  auto data2_3       = hn::Undefined(s16);
  LoadInterleaved4(s16, data, data1_0, data1_1, data1_2, data1_3);
  LoadInterleaved4(s16, data + 4 * DCTSIZE, data2_0, data2_1, data2_2, data2_3);
  auto cols_04_0 = ConcatEven(s16, data2_0, data1_0);
  auto cols_15_0 = ConcatEven(s16, data2_1, data1_1);
  auto cols_26_0 = ConcatEven(s16, data2_2, data1_2);
  auto cols_37_0 = ConcatEven(s16, data2_3, data1_3);
  auto cols_04_1 = ConcatOdd(s16, data2_0, data1_0);
  auto cols_15_1 = ConcatOdd(s16, data2_1, data1_1);
  auto cols_26_1 = ConcatOdd(s16, data2_2, data1_2);
  auto cols_37_1 = ConcatOdd(s16, data2_3, data1_3);

  auto col0 = cols_04_0;
  auto col1 = cols_15_0;
  auto col2 = cols_26_0;
  auto col3 = cols_37_0;
  auto col4 = cols_04_1;
  auto col5 = cols_15_1;
  auto col6 = cols_26_1;
  auto col7 = cols_37_1;

  auto tmp0 = Add(col0, col7);
  auto tmp7 = Sub(col0, col7);
  auto tmp1 = Add(col1, col6);
  auto tmp6 = Sub(col1, col6);
  auto tmp2 = Add(col2, col5);
  auto tmp5 = Sub(col2, col5);
  auto tmp3 = Add(col3, col4);
  auto tmp4 = Sub(col3, col4);

  // Even part
  auto tmp10 = Add(tmp0, tmp3);  // phase 2
  auto tmp13 = Sub(tmp0, tmp3);
  auto tmp11 = Add(tmp1, tmp2);
  auto tmp12 = Sub(tmp1, tmp2);

  col0 = Add(tmp10, tmp11);  // phase 3
  col4 = Sub(tmp10, tmp11);

  const auto vcoeff0 = hn::Broadcast<0>(vcoeffs);  // Set(s16, coeff[0]);
  const auto vcoeff1 = hn::Broadcast<1>(vcoeffs);  // Set(s16, coeff[1]);
  const auto vcoeff2 = hn::Broadcast<2>(vcoeffs);  // Set(s16, coeff[2]);
  const auto vcoeff3 = hn::Broadcast<3>(vcoeffs);  // Set(s16, coeff[3]);

  auto z1 = MulFixedPoint15(Add(tmp12, tmp13), vcoeff2);
  col2    = Add(tmp13, z1);  // phase 5
  col6    = Sub(tmp13, z1);

  // Odd Part
  tmp10 = Add(tmp4, tmp5);
  tmp11 = Add(tmp5, tmp6);
  tmp12 = Add(tmp6, tmp7);

  auto z5 = MulFixedPoint15(Sub(tmp10, tmp12), vcoeff0);
  auto z2 = MulFixedPoint15(tmp10, vcoeff1);
  z2      = Add(z2, z5);
  auto z4 = MulFixedPoint15(tmp12, vcoeff3);
  z5      = Add(tmp12, z5);
  z4      = Add(z4, z5);
  auto z3 = MulFixedPoint15(tmp11, vcoeff2);

  auto z11 = Add(tmp7, z3);  // phase 5
  auto z13 = Sub(tmp7, z3);

  col5 = Add(z13, z2);  // phase 6
  col3 = Sub(z13, z2);
  col1 = Add(z11, z4);
  col7 = Sub(z11, z4);

  // Transpose
  const auto q0 = InterleaveLower(s16, col0, col2);
  const auto q1 = InterleaveLower(s16, col1, col3);
  const auto q2 = InterleaveUpper(s16, col0, col2);
  const auto q3 = InterleaveUpper(s16, col1, col3);
  const auto q4 = InterleaveLower(s16, col4, col6);
  const auto q5 = InterleaveLower(s16, col5, col7);
  const auto q6 = InterleaveUpper(s16, col4, col6);
  const auto q7 = InterleaveUpper(s16, col5, col7);

  const auto r0 = InterleaveLower(s16, q0, q1);
  const auto r1 = InterleaveUpper(s16, q0, q1);
  const auto r2 = InterleaveLower(s16, q2, q3);
  const auto r3 = InterleaveUpper(s16, q2, q3);
  const auto r4 = InterleaveLower(s16, q4, q5);
  const auto r5 = InterleaveUpper(s16, q4, q5);
  const auto r6 = InterleaveLower(s16, q6, q7);
  const auto r7 = InterleaveUpper(s16, q6, q7);

  auto row0 = ConcatLowerLower(s16, r4, r0);
  auto row2 = ConcatLowerLower(s16, r5, r1);
  auto row4 = ConcatLowerLower(s16, r6, r2);
  auto row6 = ConcatLowerLower(s16, r7, r3);
  auto row1 = ConcatUpperUpper(s16, r4, r0);
  auto row3 = ConcatUpperUpper(s16, r5, r1);
  auto row5 = ConcatUpperUpper(s16, r6, r2);
  auto row7 = ConcatUpperUpper(s16, r7, r3);

  // Transpose (Old)
  //  //  vtrnq
  //  auto cols_01_0 = ZipLower(s32, ConcatEven(s16, col0, col0), ConcatEven(s16, col1, col1));
  //  auto cols_01_1 = ZipLower(s32, ConcatOdd(s16, col0, col0), ConcatOdd(s16, col1, col1));
  //  auto cols_23_0 = ZipLower(s32, ConcatEven(s16, col2, col2), ConcatEven(s16, col3, col3));
  //  auto cols_23_1 = ZipLower(s32, ConcatOdd(s16, col2, col2), ConcatOdd(s16, col3, col3));
  //  auto cols_45_0 = ZipLower(s32, ConcatEven(s16, col4, col4), ConcatEven(s16, col5, col5));
  //  auto cols_45_1 = ZipLower(s32, ConcatOdd(s16, col4, col4), ConcatOdd(s16, col5, col5));
  //  auto cols_67_0 = ZipLower(s32, ConcatEven(s16, col6, col6), ConcatEven(s16, col7, col7));
  //  auto cols_67_1 = ZipLower(s32, ConcatOdd(s16, col6, col6), ConcatOdd(s16, col7, col7));
  //
  //  auto cols_0145_l_0 =
  //      InterleaveLower(ConcatEven(s32, cols_01_0, cols_01_0), ConcatEven(s32, cols_45_0, cols_45_0));
  //  auto cols_0145_l_1 =
  //      InterleaveLower(ConcatOdd(s32, cols_01_0, cols_01_0), ConcatOdd(s32, cols_45_0, cols_45_0));
  //  auto cols_0145_h_0 =
  //      InterleaveLower(ConcatEven(s32, cols_01_1, cols_01_1), ConcatEven(s32, cols_45_1, cols_45_1));
  //  auto cols_0145_h_1 =
  //      InterleaveLower(ConcatOdd(s32, cols_01_1, cols_01_1), ConcatOdd(s32, cols_45_1, cols_45_1));
  //  auto cols_2367_l_0 =
  //      InterleaveLower(ConcatEven(s32, cols_23_0, cols_23_0), ConcatEven(s32, cols_67_0, cols_67_0));
  //  auto cols_2367_l_1 =
  //      InterleaveLower(ConcatOdd(s32, cols_23_0, cols_23_0), ConcatOdd(s32, cols_67_0, cols_67_0));
  //  auto cols_2367_h_0 =
  //      InterleaveLower(ConcatEven(s32, cols_23_1, cols_23_1), ConcatEven(s32, cols_67_1, cols_67_1));
  //  auto cols_2367_h_1 =
  //      InterleaveLower(ConcatOdd(s32, cols_23_1, cols_23_1), ConcatOdd(s32, cols_67_1, cols_67_1));
  //
  //  auto rows_04_0 = InterleaveLower(cols_0145_l_0, cols_2367_l_0);
  //  auto rows_04_1 = InterleaveUpper(s32, cols_0145_l_0, cols_2367_l_0);
  //  auto rows_15_0 = InterleaveLower(cols_0145_h_0, cols_2367_h_0);
  //  auto rows_15_1 = InterleaveUpper(s32, cols_0145_h_0, cols_2367_h_0);
  //  auto rows_26_0 = InterleaveLower(cols_0145_l_1, cols_2367_l_1);
  //  auto rows_26_1 = InterleaveUpper(s32, cols_0145_l_1, cols_2367_l_1);
  //  auto rows_37_0 = InterleaveLower(cols_0145_h_1, cols_2367_h_1);
  //  auto rows_37_1 = InterleaveUpper(s32, cols_0145_h_1, cols_2367_h_1);
  //
  //  auto row0 = BitCast(s16, rows_04_0);
  //  auto row1 = BitCast(s16, rows_15_0);
  //  auto row2 = BitCast(s16, rows_26_0);
  //  auto row3 = BitCast(s16, rows_37_0);
  //  auto row4 = BitCast(s16, rows_04_1);
  //  auto row5 = BitCast(s16, rows_15_1);
  //  auto row6 = BitCast(s16, rows_26_1);
  //  auto row7 = BitCast(s16, rows_37_1);

  /* Pass 2: process columns. */
  tmp0 = Add(row0, row7);
  tmp7 = Sub(row0, row7);
  tmp1 = Add(row1, row6);
  tmp6 = Sub(row1, row6);
  tmp2 = Add(row2, row5);
  tmp5 = Sub(row2, row5);
  tmp3 = Add(row3, row4);
  tmp4 = Sub(row3, row4);

  /* Even part */
  tmp10 = Add(tmp0, tmp3); /* phase 2 */
  tmp13 = Sub(tmp0, tmp3);
  tmp11 = Add(tmp1, tmp2);
  tmp12 = Sub(tmp1, tmp2);

  row0 = Add(tmp10, tmp11); /* phase 3 */
  row4 = Sub(tmp10, tmp11);

  z1   = MulFixedPoint15(Add(tmp12, tmp13), vcoeff2);
  row2 = Add(tmp13, z1); /* phase 5 */
  row6 = Sub(tmp13, z1);

  /* Odd part */
  tmp10 = Add(tmp4, tmp5); /* phase 2 */
  tmp11 = Add(tmp5, tmp6);
  tmp12 = Add(tmp6, tmp7);

  z5 = MulFixedPoint15(Sub(tmp10, tmp12), vcoeff0);
  z2 = MulFixedPoint15(tmp10, vcoeff1);
  z2 = Add(z2, z5);
  z4 = MulFixedPoint15(tmp12, vcoeff3);
  z5 = Add(tmp12, z5);
  z4 = Add(z4, z5);
  z3 = MulFixedPoint15(tmp11, vcoeff2);

  z11 = Add(tmp7, z3); /* phase 5 */
  z13 = Sub(tmp7, z3);

  row5 = Add(z13, z2); /* phase 6 */
  row3 = Sub(z13, z2);
  row1 = Add(z11, z4);
  row7 = Sub(z11, z4);

  Store(row0, s16, data + 0 * DCTSIZE);
  Store(row1, s16, data + 1 * DCTSIZE);
  Store(row2, s16, data + 2 * DCTSIZE);
  Store(row3, s16, data + 3 * DCTSIZE);
  Store(row4, s16, data + 4 * DCTSIZE);
  Store(row5, s16, data + 5 * DCTSIZE);
  Store(row6, s16, data + 6 * DCTSIZE);
  Store(row7, s16, data + 7 * DCTSIZE);
#else
  int32_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;
  int32_t tmp10, tmp11, tmp12, tmp13;
  int32_t z1, z2, z3, z4, z5, z11, z13;
  int16_t *dataptr;
  int ctr;
  constexpr int half = 1 << 14;

  /* Pass 1: process rows. */

  dataptr = data;
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

    dataptr[0] = static_cast<int16_t>((tmp10 + tmp11)); /* phase 3 */
    dataptr[4] = static_cast<int16_t>((tmp10 - tmp11));

    z1         = ((int32_t)(tmp12 + tmp13) * coeff[2] + half) >> 15; /* c4 */
    dataptr[2] = static_cast<int16_t>((tmp13 + z1));                 /* phase 5 */
    dataptr[6] = static_cast<int16_t>((tmp13 - z1));

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((int32_t)(tmp10 - tmp12) * coeff[0] + half) >> 15;      /* c6 */
    z2 = ((int32_t)(coeff[1] * tmp10 + half) >> 15) + z5;         /* c2-c6 */
    z4 = ((int32_t)(coeff[3] * tmp12 + half) >> 15) + z5 + tmp12; /* c2+c6 */
    z3 = ((int32_t)tmp11 * coeff[2] + half) >> 15;                /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[5] = static_cast<int16_t>((z13 + z2)); /* phase 6 */
    dataptr[3] = static_cast<int16_t>((z13 - z2));
    dataptr[1] = static_cast<int16_t>((z11 + z4));
    dataptr[7] = static_cast<int16_t>((z11 - z4));

    dataptr += DCTSIZE; /* advance pointer to next row */
  }
  /* Pass 2: process columns. */

  dataptr = data;

  for (ctr = 8 - 1; ctr >= 0; ctr--) {
    tmp0 = dataptr[DCTSIZE * 0] + dataptr[DCTSIZE * 7];
    tmp7 = dataptr[DCTSIZE * 0] - dataptr[DCTSIZE * 7];
    tmp1 = dataptr[DCTSIZE * 1] + dataptr[DCTSIZE * 6];
    tmp6 = dataptr[DCTSIZE * 1] - dataptr[DCTSIZE * 6];
    tmp2 = dataptr[DCTSIZE * 2] + dataptr[DCTSIZE * 5];
    tmp5 = dataptr[DCTSIZE * 2] - dataptr[DCTSIZE * 5];
    tmp3 = dataptr[DCTSIZE * 3] + dataptr[DCTSIZE * 4];
    tmp4 = dataptr[DCTSIZE * 3] - dataptr[DCTSIZE * 4];

    /* Even part */

    tmp10 = tmp0 + tmp3; /* phase 2 */
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;

    dataptr[DCTSIZE * 0] = static_cast<int16_t>((tmp10 + tmp11)); /* phase 3 */
    dataptr[DCTSIZE * 4] = static_cast<int16_t>((tmp10 - tmp11));

    z1                   = ((int32_t)(tmp12 + tmp13) * coeff[2] + half) >> 15; /* c4 */
    dataptr[DCTSIZE * 2] = static_cast<int16_t>((tmp13 + z1));                 /* phase 5 */
    dataptr[DCTSIZE * 6] = static_cast<int16_t>((tmp13 - z1));

    /* Odd part */

    tmp10 = tmp4 + tmp5; /* phase 2 */
    tmp11 = tmp5 + tmp6;
    tmp12 = tmp6 + tmp7;

    /* The rotator is modified from fig 4-8 to avoid extra negations. */
    z5 = ((int32_t)(tmp10 - tmp12) * coeff[0] + half) >> 15;      /* c6 */
    z2 = ((int32_t)(coeff[1] * tmp10 + half) >> 15) + z5;         /* c2-c6 */
    z4 = ((int32_t)(coeff[3] * tmp12 + half) >> 15) + z5 + tmp12; /* c2+c6 */
    z3 = ((int32_t)tmp11 * coeff[2] + half) >> 15;                /* c4 */

    z11 = tmp7 + z3; /* phase 5 */
    z13 = tmp7 - z3;

    dataptr[DCTSIZE * 5] = static_cast<int16_t>((z13 + z2)); /* phase 6 */
    dataptr[DCTSIZE * 3] = static_cast<int16_t>((z13 - z2));
    dataptr[DCTSIZE * 1] = static_cast<int16_t>((z11 + z4));
    dataptr[DCTSIZE * 7] = static_cast<int16_t>((z11 - z4));

    dataptr++; /* advance pointer to next column */
  }
#endif
}
