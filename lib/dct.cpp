// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dct.cpp"  // this file
#include <hwy/foreach_target.h>       // must come before highway.h
#include <hwy/highway.h>

#include "dct.hpp"
#include "constants.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ALIGN static const int16_t coeff[] = {12544, 17792, 23168, 9984};

HWY_ATTR void fast_dct2_simd(int16_t *HWY_RESTRICT data) {
  HWY_CAPPED(int16_t, 8) s16;
  HWY_CAPPED(int32_t, 4) s32;
  auto data1_0 = hn::Undefined(s16);
  auto data1_1 = hn::Undefined(s16);
  auto data1_2 = hn::Undefined(s16);
  auto data1_3 = hn::Undefined(s16);
  auto data2_0 = hn::Undefined(s16);
  auto data2_1 = hn::Undefined(s16);
  auto data2_2 = hn::Undefined(s16);
  auto data2_3 = hn::Undefined(s16);
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

  auto vcoeff0 = Set(s16, coeff[0]);
  auto vcoeff1 = Set(s16, coeff[1]);
  auto vcoeff2 = Set(s16, coeff[2]);
  auto vcoeff3 = Set(s16, coeff[3]);
  auto z1      = MulFixedPoint15(Add(tmp12, tmp13), vcoeff2);
  col2         = Add(tmp13, z1);  // phase 5
  col6         = Sub(tmp13, z1);

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

  // vtrnq
  auto cols_01_0 = InterleaveLower(ConcatEven(s16, col0, col0), ConcatEven(s16, col1, col1));
  auto cols_01_1 = InterleaveLower(ConcatOdd(s16, col0, col0), ConcatOdd(s16, col1, col1));
  auto cols_23_0 = InterleaveLower(ConcatEven(s16, col2, col2), ConcatEven(s16, col3, col3));
  auto cols_23_1 = InterleaveLower(ConcatOdd(s16, col2, col2), ConcatOdd(s16, col3, col3));
  auto cols_45_0 = InterleaveLower(ConcatEven(s16, col4, col4), ConcatEven(s16, col5, col5));
  auto cols_45_1 = InterleaveLower(ConcatOdd(s16, col4, col4), ConcatOdd(s16, col5, col5));
  auto cols_67_0 = InterleaveLower(ConcatEven(s16, col6, col6), ConcatEven(s16, col7, col7));
  auto cols_67_1 = InterleaveLower(ConcatOdd(s16, col6, col6), ConcatOdd(s16, col7, col7));

  auto cols_0145_l_0 = InterleaveLower(ConcatEven(s32, BitCast(s32, cols_01_0), BitCast(s32, cols_01_0)),
                                       ConcatEven(s32, BitCast(s32, cols_45_0), BitCast(s32, cols_45_0)));
  auto cols_0145_l_1 = InterleaveLower(ConcatOdd(s32, BitCast(s32, cols_01_0), BitCast(s32, cols_01_0)),
                                       ConcatOdd(s32, BitCast(s32, cols_45_0), BitCast(s32, cols_45_0)));
  auto cols_0145_h_0 = InterleaveLower(ConcatEven(s32, BitCast(s32, cols_01_1), BitCast(s32, cols_01_1)),
                                       ConcatEven(s32, BitCast(s32, cols_45_1), BitCast(s32, cols_45_1)));
  auto cols_0145_h_1 = InterleaveLower(ConcatOdd(s32, BitCast(s32, cols_01_1), BitCast(s32, cols_01_1)),
                                       ConcatOdd(s32, BitCast(s32, cols_45_1), BitCast(s32, cols_45_1)));
  auto cols_2367_l_0 = InterleaveLower(ConcatEven(s32, BitCast(s32, cols_23_0), BitCast(s32, cols_23_0)),
                                       ConcatEven(s32, BitCast(s32, cols_67_0), BitCast(s32, cols_67_0)));
  auto cols_2367_l_1 = InterleaveLower(ConcatOdd(s32, BitCast(s32, cols_23_0), BitCast(s32, cols_23_0)),
                                       ConcatOdd(s32, BitCast(s32, cols_67_0), BitCast(s32, cols_67_0)));
  auto cols_2367_h_0 = InterleaveLower(ConcatEven(s32, BitCast(s32, cols_23_1), BitCast(s32, cols_23_1)),
                                       ConcatEven(s32, BitCast(s32, cols_67_1), BitCast(s32, cols_67_1)));
  auto cols_2367_h_1 = InterleaveLower(ConcatOdd(s32, BitCast(s32, cols_23_1), BitCast(s32, cols_23_1)),
                                       ConcatOdd(s32, BitCast(s32, cols_67_1), BitCast(s32, cols_67_1)));
  auto rows_04_0     = InterleaveLower(cols_0145_l_0, cols_2367_l_0);
  auto rows_04_1     = InterleaveUpper(s32, cols_0145_l_0, cols_2367_l_0);
  auto rows_15_0     = InterleaveLower(cols_0145_h_0, cols_2367_h_0);
  auto rows_15_1     = InterleaveUpper(s32, cols_0145_h_0, cols_2367_h_0);
  auto rows_26_0     = InterleaveLower(cols_0145_l_1, cols_2367_l_1);
  auto rows_26_1     = InterleaveUpper(s32, cols_0145_l_1, cols_2367_l_1);
  auto rows_37_0     = InterleaveLower(cols_0145_h_1, cols_2367_h_1);
  auto rows_37_1     = InterleaveUpper(s32, cols_0145_h_1, cols_2367_h_1);

  auto row0 = BitCast(s16, rows_04_0);
  auto row1 = BitCast(s16, rows_15_0);
  auto row2 = BitCast(s16, rows_26_0);
  auto row3 = BitCast(s16, rows_37_0);
  auto row4 = BitCast(s16, rows_04_1);
  auto row5 = BitCast(s16, rows_15_1);
  auto row6 = BitCast(s16, rows_26_1);
  auto row7 = BitCast(s16, rows_37_1);

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
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(fast_dct2_simd);
void dct2(std::vector<int16_t *> in, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;

  for (int i = 0; i < width * LINES; i += DCTSIZE2) {
    HWY_DYNAMIC_DISPATCH(fast_dct2_simd)(in[0] + i);
  }
  for (int c = 1; c < nc; ++c) {
    for (int i = 0; i < width / scale_x * LINES / scale_y; i += DCTSIZE2) {
      HWY_DYNAMIC_DISPATCH(fast_dct2_simd)(in[c] + i);
    }
  }
}

}  // namespace jpegenc_hwy
#endif