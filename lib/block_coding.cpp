// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "block_coding.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

// clang-format off
HWY_ALIGN int16_t indices[] = {
        0, 1, 8, 0, 9, 2, 3, 10,
        0, 0, 0, 0, 0, 11, 4, 5,
        1, 8, 0, 9, 2, 0, 0, 0,
        0, 0, 0, 1, 8, 0, 9, 2,
        11, 4, 0, 0, 0, 0, 5, 12,
        0, 0, 13, 6, 7, 14, 0, 0,
        3, 10, 0, 0, 0, 0, 11, 4,
        0, 0, 1, 8, 9, 2, 0, 0,
        13, 6, 0, 7, 14, 0, 0, 0,
        0, 0, 0, 13, 6, 0, 7, 14,
        10, 11, 4, 0, 0, 0, 0, 0,
        5, 12, 13, 6, 0, 7, 14, 15
};
// clang-format on
#if HWY_TARGET != HWY_SCALAR
HWY_ATTR void make_zigzag_blk(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
  HWY_CAPPED(uint8_t, 16) u8;
  HWY_CAPPED(uint8_t, 8) u8_64;
  HWY_CAPPED(uint64_t, 1) u64_64;
  HWY_CAPPED(uint16_t, 8) u16;
  HWY_CAPPED(int16_t, 8) s16;

  HWY_ALIGN int16_t dp[64];
  int dc  = sp[0];
  sp[0]   = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc = dc;

  auto v0 = hn::Load(s16, sp);
  auto v1 = hn::Load(s16, sp + 8);
  auto v2 = hn::Load(s16, sp + 16);
  auto v3 = hn::Load(s16, sp + 24);
  auto v4 = hn::Load(s16, sp + 32);
  auto v5 = hn::Load(s16, sp + 40);
  auto v6 = hn::Load(s16, sp + 48);
  auto v7 = hn::Load(s16, sp + 56);

  auto row0   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[0 * 8]));
  row0        = InsertLane(row0, 3, ExtractLane(v2, 0));
  auto row1   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[1 * 8]));
  auto row1_1 = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[2 * 8]));
  auto row2   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[3 * 8]));
  auto row3   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[4 * 8]));
  auto row3_1 = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[5 * 8]));
  auto row4   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[6 * 8]));
  auto row4_1 = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[7 * 8]));
  auto row5   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[8 * 8]));
  auto row6   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[9 * 8]));
  auto row6_1 = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[10 * 8]));
  auto row7   = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[11 * 8]));
  row7        = InsertLane(row7, 4, ExtractLane(v5, 7));

  auto m5                     = FirstN(s16, 5);
  auto m3                     = FirstN(s16, 3);
  HWY_ALIGN uint8_t mask34[8] = {0b00111100};
  auto m34                    = LoadMaskBits(s16, mask34);

  row1   = IfThenZeroElse(m5, row1);
  row1_1 = IfThenElseZero(m5, row1_1);
  row1   = Or(row1, row1_1);
  row1   = InsertLane(row1, 2, ExtractLane(v4, 0));
  row2   = IfThenZeroElse(m3, row2);
  row2   = InsertLane(row2, 0, ExtractLane(v1, 4));
  row2   = InsertLane(row2, 1, ExtractLane(v2, 3));
  row2   = InsertLane(row2, 2, ExtractLane(v3, 2));
  row2   = InsertLane(row2, 5, ExtractLane(v6, 0));
  row3   = IfThenZeroElse(m34, row3);
  row3_1 = IfThenElseZero(m34, row3_1);
  row3   = Or(row3, row3_1);
  row4   = IfThenZeroElse(m34, row4);
  row4_1 = IfThenElseZero(m34, row4_1);
  row4   = Or(row4, row4_1);
  row5   = IfThenZeroElse(Not(m5), row5);
  row5   = InsertLane(row5, 2, ExtractLane(v1, 7));
  row5   = InsertLane(row5, 5, ExtractLane(v4, 5));
  row5   = InsertLane(row5, 6, ExtractLane(v5, 4));
  row5   = InsertLane(row5, 7, ExtractLane(v6, 3));
  row6   = IfThenZeroElse(m3, row6);
  row6_1 = IfThenElseZero(m3, row6_1);
  row6   = Or(row6, row6_1);
  row6   = InsertLane(row6, 5, ExtractLane(v3, 7));
  /* DCT block is now in zig-zag order; start Huffman encoding process. */

  /* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
   * means that the corresponding coefficient != 0.
   */
  auto zero       = Zero(s16);
  auto row0_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row0), zero));
  auto row1_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row1), zero));
  auto row2_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row2), zero));
  auto row3_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row3), zero));
  auto row4_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row4), zero));
  auto row5_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row5), zero));
  auto row6_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row6), zero));
  auto row7_ne_0  = VecFromMask(s16, Lt(HighestSetBitIndex(row7), zero));
  auto row10_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row0_ne_0), ResizeBitCast(u8, row1_ne_0));
  auto row32_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row2_ne_0), ResizeBitCast(u8, row3_ne_0));
  auto row54_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row4_ne_0), ResizeBitCast(u8, row5_ne_0));
  auto row76_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row6_ne_0), ResizeBitCast(u8, row7_ne_0));

  /* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
  HWY_ALIGN uint8_t bm[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
                            0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

  auto bitmap_mask = Load(u8, bm);

  auto bitmap_rows_10 = AndNot(row10_ne_0, bitmap_mask);
  auto bitmap_rows_32 = AndNot(row32_ne_0, bitmap_mask);
  auto bitmap_rows_54 = AndNot(row54_ne_0, bitmap_mask);
  auto bitmap_rows_76 = AndNot(row76_ne_0, bitmap_mask);

  auto bitmap_rows_3210     = Padd(u8, bitmap_rows_32, bitmap_rows_10);
  auto bitmap_rows_7654     = Padd(u8, bitmap_rows_76, bitmap_rows_54);
  auto bitmap_rows_76543210 = Padd(u8, bitmap_rows_7654, bitmap_rows_3210);
  auto bitmap_all = Padd(u8_64, LowerHalf(bitmap_rows_76543210), UpperHalf(u8, bitmap_rows_76543210));
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = GetLane(BitCast(u64_64, bitmap_all));
  HWY_ALIGN uint8_t bits[64];

  auto abs_row0 = Abs(row0);
  auto abs_row1 = Abs(row1);
  auto abs_row2 = Abs(row2);
  auto abs_row3 = Abs(row3);
  auto abs_row4 = Abs(row4);
  auto abs_row5 = Abs(row5);
  auto abs_row6 = Abs(row6);
  auto abs_row7 = Abs(row7);

  auto row0_lz = LeadingZeroCount(abs_row0);
  auto row1_lz = LeadingZeroCount(abs_row1);
  auto row2_lz = LeadingZeroCount(abs_row2);
  auto row3_lz = LeadingZeroCount(abs_row3);
  auto row4_lz = LeadingZeroCount(abs_row4);
  auto row5_lz = LeadingZeroCount(abs_row5);
  auto row6_lz = LeadingZeroCount(abs_row6);
  auto row7_lz = LeadingZeroCount(abs_row7);

  /* Narrow leading zero count to 8 bits. */
  auto row01_lz = ConcatEven(u8, ResizeBitCast(u8, row1_lz), ResizeBitCast(u8, row0_lz));
  auto row23_lz = ConcatEven(u8, ResizeBitCast(u8, row3_lz), ResizeBitCast(u8, row2_lz));
  auto row45_lz = ConcatEven(u8, ResizeBitCast(u8, row5_lz), ResizeBitCast(u8, row4_lz));
  auto row67_lz = ConcatEven(u8, ResizeBitCast(u8, row7_lz), ResizeBitCast(u8, row6_lz));
  /* Compute nbits needed to specify magnitude of each coefficient. */
  auto row01_nbits = Sub(Set(u8, 16), row01_lz);
  auto row23_nbits = Sub(Set(u8, 16), row23_lz);
  auto row45_nbits = Sub(Set(u8, 16), row45_lz);
  auto row67_nbits = Sub(Set(u8, 16), row67_lz);
  /* Store nbits. */
  Store(row01_nbits, u8, bits + 0 * DCTSIZE);
  Store(row23_nbits, u8, bits + 2 * DCTSIZE);
  Store(row45_nbits, u8, bits + 4 * DCTSIZE);
  Store(row67_nbits, u8, bits + 6 * DCTSIZE);

  auto row0_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row0, zero))), BitCast(u16, row0_lz));
  auto row1_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row1, zero))), BitCast(u16, row1_lz));
  auto row2_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row2, zero))), BitCast(u16, row2_lz));
  auto row3_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row3, zero))), BitCast(u16, row3_lz));
  auto row4_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row4, zero))), BitCast(u16, row4_lz));
  auto row5_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row5, zero))), BitCast(u16, row5_lz));
  auto row6_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row6, zero))), BitCast(u16, row6_lz));
  auto row7_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row7, zero))), BitCast(u16, row7_lz));

  auto row0_diff = Xor(BitCast(u16, abs_row0), row0_mask);
  auto row1_diff = Xor(BitCast(u16, abs_row1), row1_mask);
  auto row2_diff = Xor(BitCast(u16, abs_row2), row2_mask);
  auto row3_diff = Xor(BitCast(u16, abs_row3), row3_mask);
  auto row4_diff = Xor(BitCast(u16, abs_row4), row4_mask);
  auto row5_diff = Xor(BitCast(u16, abs_row5), row5_mask);
  auto row6_diff = Xor(BitCast(u16, abs_row6), row6_mask);
  auto row7_diff = Xor(BitCast(u16, abs_row7), row7_mask);

  Store(BitCast(s16, row0_diff), s16, dp + 0 * DCTSIZE);
  Store(BitCast(s16, row1_diff), s16, dp + 1 * DCTSIZE);
  Store(BitCast(s16, row2_diff), s16, dp + 2 * DCTSIZE);
  Store(BitCast(s16, row3_diff), s16, dp + 3 * DCTSIZE);
  Store(BitCast(s16, row4_diff), s16, dp + 4 * DCTSIZE);
  Store(BitCast(s16, row5_diff), s16, dp + 5 * DCTSIZE);
  Store(BitCast(s16, row6_diff), s16, dp + 6 * DCTSIZE);
  Store(BitCast(s16, row7_diff), s16, dp + 7 * DCTSIZE);

  bitmap <<= 1;

  // EncodeDC
  enc.put_bits(tab.DC_cwd[bits[0]], tab.DC_len[bits[0]]);
  if (bits[0] != 0) {
    enc.put_bits(dp[0], bits[0]);
  }

  int count = 1;
  int run;
  while (bitmap != 0) {
    run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // ZRL
      enc.put_bits(tab.AC_cwd[0xF0], tab.AC_len[0xF0]);
      run -= 16;
    }
    // EncodeAC
    enc.put_bits(tab.AC_cwd[(run << 4) + bits[count]], tab.AC_len[(run << 4) + bits[count]]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EOB
    enc.put_bits(tab.AC_cwd[0x00], tab.AC_len[0x00]);
  }
}
#else
  #include "zigzag_order.hpp"

HWY_ATTR void make_zigzag_blk(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
  alignas(16) int16_t dp[64];
  int dc          = sp[0];
  sp[0]           = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc         = dc;
  uint64_t bitmap = 0;
  for (int i = 0; i < DCTSIZE2; ++i) {
    dp[i] = sp[scan[i]];
    bitmap |= (dp[i] != 0);
    bitmap <<= 1;
  }
  int32_t s;
  //  Branchless abs:
  //  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
  uint32_t uval = (dp[0] + (dp[0] >> 15)) ^ (dp[0] >> 15);
  s             = 32 - JPEGENC_CLZ32(uval);

  //  EncodeDC
  enc.put_bits(tab.DC_cwd[s], tab.DC_len[s]);
  if (s != 0) {
    dp[0] -= (dp[0] >> 15) & 1;
    enc.put_bits(dp[0], s);
  }

  int count = 1;
  int run;
  while (bitmap != 0) {
    run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // ZRL
      enc.put_bits(tab.AC_cwd[0xF0], tab.AC_len[0xF0]);
      run -= 16;
    }
    // Encode AC
    uval = (dp[count] + (dp[count] >> 15)) ^ (dp[count] >> 15);
    s    = 32 - JPEGENC_CLZ32(uval);
    enc.put_bits(tab.AC_cwd[(run << 4) + s], tab.AC_len[(run << 4) + s]);
    dp[count] -= (dp[count] >> 15) & 1;
    enc.put_bits(dp[count], s);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EOB
    enc.put_bits(tab.AC_cwd[0x00], tab.AC_len[0x00]);
  }
}
#endif

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(make_zigzag_blk);
void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 huff_info &tab_Y, huff_info &tab_C, bitstream &enc) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int stride;
  int16_t *sp0, *sp1, *sp2;

  stride = width * DCTSIZE;
  if (nc == 3) {
    sp1 = in[1];
    sp2 = in[2];
    for (int Ly = 0, Cy = 0; Ly < LINES / DCTSIZE; Ly += Vl, ++Cy) {
      for (int Lx = 0, Cx = 0; Lx < width / DCTSIZE; Lx += Hl, ++Cx) {
        // Luma, Y
        for (int y = 0; y < Vl; ++y) {
          for (int x = 0; x < Hl; ++x) {
            sp0 = in[0] + (Ly + y) * stride + (Lx + x) * DCTSIZE2;  // top-left of an MCU
            HWY_DYNAMIC_DISPATCH(make_zigzag_blk)(sp0, tab_Y, prev_dc[0], enc);
          }
        }
        // Chroma, Cb
        HWY_DYNAMIC_DISPATCH(make_zigzag_blk)(sp1, tab_C, prev_dc[1], enc);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        HWY_DYNAMIC_DISPATCH(make_zigzag_blk)(sp2, tab_C, prev_dc[2], enc);
        sp2 += DCTSIZE2;
      }
    }
  } else {
    sp0 = in[0];
    for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        HWY_DYNAMIC_DISPATCH(make_zigzag_blk)(sp0, tab_Y, prev_dc[0], enc);
        sp0 += DCTSIZE2;
      }
    }
  }
}
}  // namespace jpegenc_hwy
#endif