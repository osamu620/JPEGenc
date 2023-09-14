// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "block_coding.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include <cstring>

#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"
#include "hwy/aligned_allocator.h"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
/* The following DCT algorithm is derived from
 * Yukihiro Arai, Takeshi Agui, and Masayuki Nakajima, "A Fast DCT-SQ Scheme for Images, "
 * IEICE Transactions on Fundamentals of Electronics, Communications and Computer Sciences 71 (1988),
   1095--1097.
 */

//  coeff  = {0.382683432, 0.541196100, 0.707106718, 1.306562963 - 1.0} * 2^15
HWY_ALIGN static const int16_t coeff[] = {12540, 17734, 23170, 10045};

HWY_ATTR void dct2_core(int16_t *HWY_RESTRICT data) {
#if HWY_TARGET != HWY_SCALAR
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

  //  vtrnq
  auto cols_01_0 = ZipLower(s32, ConcatEven(s16, col0, col0), ConcatEven(s16, col1, col1));
  auto cols_01_1 = ZipLower(s32, ConcatOdd(s16, col0, col0), ConcatOdd(s16, col1, col1));
  auto cols_23_0 = ZipLower(s32, ConcatEven(s16, col2, col2), ConcatEven(s16, col3, col3));
  auto cols_23_1 = ZipLower(s32, ConcatOdd(s16, col2, col2), ConcatOdd(s16, col3, col3));
  auto cols_45_0 = ZipLower(s32, ConcatEven(s16, col4, col4), ConcatEven(s16, col5, col5));
  auto cols_45_1 = ZipLower(s32, ConcatOdd(s16, col4, col4), ConcatOdd(s16, col5, col5));
  auto cols_67_0 = ZipLower(s32, ConcatEven(s16, col6, col6), ConcatEven(s16, col7, col7));
  auto cols_67_1 = ZipLower(s32, ConcatOdd(s16, col6, col6), ConcatOdd(s16, col7, col7));

  auto cols_0145_l_0 =
      InterleaveLower(ConcatEven(s32, cols_01_0, cols_01_0), ConcatEven(s32, cols_45_0, cols_45_0));
  auto cols_0145_l_1 =
      InterleaveLower(ConcatOdd(s32, cols_01_0, cols_01_0), ConcatOdd(s32, cols_45_0, cols_45_0));
  auto cols_0145_h_0 =
      InterleaveLower(ConcatEven(s32, cols_01_1, cols_01_1), ConcatEven(s32, cols_45_1, cols_45_1));
  auto cols_0145_h_1 =
      InterleaveLower(ConcatOdd(s32, cols_01_1, cols_01_1), ConcatOdd(s32, cols_45_1, cols_45_1));
  auto cols_2367_l_0 =
      InterleaveLower(ConcatEven(s32, cols_23_0, cols_23_0), ConcatEven(s32, cols_67_0, cols_67_0));
  auto cols_2367_l_1 =
      InterleaveLower(ConcatOdd(s32, cols_23_0, cols_23_0), ConcatOdd(s32, cols_67_0, cols_67_0));
  auto cols_2367_h_0 =
      InterleaveLower(ConcatEven(s32, cols_23_1, cols_23_1), ConcatEven(s32, cols_67_1, cols_67_1));
  auto cols_2367_h_1 =
      InterleaveLower(ConcatOdd(s32, cols_23_1, cols_23_1), ConcatOdd(s32, cols_67_1, cols_67_1));

  auto rows_04_0 = InterleaveLower(cols_0145_l_0, cols_2367_l_0);
  auto rows_04_1 = InterleaveUpper(s32, cols_0145_l_0, cols_2367_l_0);
  auto rows_15_0 = InterleaveLower(cols_0145_h_0, cols_2367_h_0);
  auto rows_15_1 = InterleaveUpper(s32, cols_0145_h_0, cols_2367_h_0);
  auto rows_26_0 = InterleaveLower(cols_0145_l_1, cols_2367_l_1);
  auto rows_26_1 = InterleaveUpper(s32, cols_0145_l_1, cols_2367_l_1);
  auto rows_37_0 = InterleaveLower(cols_0145_h_1, cols_2367_h_1);
  auto rows_37_1 = InterleaveUpper(s32, cols_0145_h_1, cols_2367_h_1);

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

HWY_ATTR void quantize_core(int16_t *HWY_RESTRICT data, const int *HWY_RESTRICT qtable) {
#if HWY_TARGET != HWY_SCALAR
  const hn::ScalableTag<int16_t> d16;
  const hn::ScalableTag<int32_t> d32;
  auto half = hn::Set(d32, 1 << 15);
  for (int i = 0; i < DCTSIZE2; i += Lanes(d16)) {
    auto ql = Load(d32, qtable + i);
    auto qh = Load(d32, qtable + i + Lanes(d32));
    auto v  = Load(d16, data + i);
    auto vl = PromoteLowerTo(d32, v);
    auto vh = PromoteUpperTo(d32, v);

    vl = MulAdd(vl, ql, half);
    vh = MulAdd(vh, qh, half);
    vl = hn::ShiftRight<16>(vl);
    vh = hn::ShiftRight<16>(vh);
    Store(OrderedDemote2To(d16, vl, vh), d16, data + i);
  }
#else
  int shift = 16;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < DCTSIZE2; ++i) {
    data[i] = static_cast<int16_t>((data[i] * qtable[i] + half) >> shift);
  }
#endif
}
HWY_ATTR void EncodeSingleBlock(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
  int dc  = sp[0];
  sp[0]   = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc = dc;

#if HWY_TARGET != HWY_SCALAR
  uint64_t bitmap;
  HWY_ALIGN int16_t dp[64];
  HWY_ALIGN uint8_t bits[64];

  using namespace hn;
  const ScalableTag<int16_t> s16;
  const ScalableTag<uint16_t> u16;
  const ScalableTag<uint8_t> u8;
  const ScalableTag<uint64_t> u64;

  #if HWY_MAX_BYTES == 64
    #include "block_coding_512.cpp"
  #elif HWY_MAX_BYTES == 32
    #include "block_coding_256.cpp"
  #else
    #include "block_coding_128.cpp"
  #endif

  // EncodeDC
  enc.put_bits(tab.DC_cwd[bits[0]], tab.DC_len[bits[0]]);
  if (bitmap & 0x8000000000000000) {
    enc.put_bits(dp[0], bits[0]);
  }
  bitmap <<= 1;

  int count = 1;
  while (bitmap != 0) {
    int run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // ZRL
      enc.put_bits(tab.AC_cwd[0xF0], tab.AC_len[0xF0]);
      run -= 16;
    }
    // EncodeAC
    size_t RS = (run << 4) + bits[count];
    enc.put_bits(tab.AC_cwd[RS], tab.AC_len[RS]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EOB
    enc.put_bits(tab.AC_cwd[0x00], tab.AC_len[0x00]);
  }

#else
  #include "block_coding_scalar.cpp"
#endif
}

HWY_ATTR void make_zigzag_blk(std::vector<int16_t *> &in, int16_t *HWY_RESTRICT mcu, int width,
                              const int mcu_height, const int YCCtype, int *HWY_RESTRICT qtable,
                              std::vector<int> &prev_dc, huff_info &tab_Y, huff_info &tab_C,
                              bitstream &enc) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;

  const int num_mcus = (mcu_height / (DCTSIZE)) * (width / (DCTSIZE));
  const int mcu_skip = Hl * Vl;

  int16_t *ssp0 = in[0];
  int16_t *ssp1 = in[1];
  int16_t *ssp2 = in[2];

  int16_t *sp;

  if (nc == 3) {  // color
    for (int k = 0; k < num_mcus; k += mcu_skip) {
      sp = mcu;
      memcpy(sp, ssp0, sizeof(int16_t) * DCTSIZE2 * mcu_skip);
      memcpy(sp + DCTSIZE2 * mcu_skip, ssp1, sizeof(int16_t) * DCTSIZE2);
      memcpy(sp + DCTSIZE2 * mcu_skip + DCTSIZE2, ssp2, sizeof(int16_t) * DCTSIZE2);
      ssp0 += DCTSIZE2 * mcu_skip;
      ssp1 += DCTSIZE2;
      ssp2 += DCTSIZE2;
      // Luma, Y
      for (int i = mcu_skip; i > 0; --i) {
        dct2_core(sp);
        quantize_core(sp, qtable);
        sp += DCTSIZE2;
      }
      // Chroma, Cb
      dct2_core(sp);
      quantize_core(sp, qtable + DCTSIZE2);
      sp += DCTSIZE2;
      // Chroma, Cr
      dct2_core(sp);
      quantize_core(sp, qtable + DCTSIZE2);

      sp -= DCTSIZE2 * mcu_skip + DCTSIZE2;
      // Luma, Y
      for (int i = mcu_skip; i > 0; --i) {
        EncodeSingleBlock(sp, tab_Y, prev_dc[0], enc);
        sp += DCTSIZE2;
      }
      // Chroma, Cb
      EncodeSingleBlock(sp, tab_C, prev_dc[1], enc);
      sp += DCTSIZE2;
      // Chroma, Cr
      EncodeSingleBlock(sp, tab_C, prev_dc[2], enc);
    }
  } else {  // monochrome
    sp = mcu;
    for (int k = 0; k < num_mcus; k += mcu_skip) {
      memcpy(sp, in[0], DCTSIZE2);
      ssp0 += DCTSIZE2;
      // Luma, Y
      dct2_core(sp);
      quantize_core(sp, qtable);
      EncodeSingleBlock(sp, tab_Y, prev_dc[0], enc);
      //      sp0 += DCTSIZE2;
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(make_zigzag_blk);
void Encode_MCUs(std::vector<int16_t *> &in, int16_t *HWY_RESTRICT mcu, int width, const int mcu_height,
                 const int YCCtype, int *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &tab_Y,
                 huff_info &tab_C, bitstream &enc) {
  HWY_DYNAMIC_DISPATCH(make_zigzag_blk)
  (in, mcu, width, mcu_height, YCCtype, qtable, prev_dc, tab_Y, tab_C, enc);
}
}  // namespace jpegenc_hwy
#endif