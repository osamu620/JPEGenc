#include <hwy/highway.h>

#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"

namespace hn = hwy::HWY_NAMESPACE;

namespace jpegenc_hwy {

// clang-format off
    alignas(16) int16_t indices[]  =  {
            0,  1,  8,  0,  9,  2,  3, 10,
            0,  0,  0,  0,  0, 11,  4,  5,
            1,  8,  0,  9,  2,  0,  0,  0,
            0,  0,  0,  1,  8,  0,  9,  2,
            11,  4,  0,  0,  0,  0,  5, 12,
            0,  0, 13,  6,  7, 14,  0,  0,
            3, 10,  0,  0,  0,  0, 11,  4,
            0,  0,  1,  8,  9,  2,  0,  0,
            13,  6,  0,  7, 14,  0,  0,  0,
            0,  0,  0, 13,  6,  0,  7, 14,
            10, 11,  4,  0,  0,  0,  0,  0,
            5, 12, 13,  6,  0,  7, 14, 15
    };
// clang-format on

#define Padd(d, V2, V1) ConcatEven((d), Add(DupEven((V1)), DupOdd((V1))), Add(DupEven((V2)), DupOdd((V2))))

static void make_zigzag_blk_simd(int16_t *HWY_RESTRICT sp, int c, int &prev_dc, bitstream &enc) {
  const hn::FixedTag<uint8_t, 16> u8;
  const hn::FixedTag<uint8_t, 8> u8_64;
  const hn::FixedTag<uint64_t, 1> u64_64;
  const hn::FixedTag<uint16_t, 8> u16;
  const hn::FixedTag<int16_t, 8> s16;
  const hn::FixedTag<int32_t, 4> s32;
  alignas(16) int16_t dp[64];
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

  auto m5                       = FirstN(s16, 5);
  auto m3                       = FirstN(s16, 3);
  alignas(16) uint8_t mask34[8] = {0b00111100};
  auto m34                      = LoadMaskBits(s16, mask34);

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
  auto row0_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row0), zero)));
  auto row1_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row1), zero)));
  auto row2_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row2), zero)));
  auto row3_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row3), zero)));
  auto row4_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row4), zero)));
  auto row5_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row5), zero)));
  auto row6_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row6), zero)));
  auto row7_ne_0  = VecFromMask(s16, Not(Lt(HighestSetBitIndex(row7), zero)));
  auto row10_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row0_ne_0), ResizeBitCast(u8, row1_ne_0));
  auto row32_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row2_ne_0), ResizeBitCast(u8, row3_ne_0));
  auto row54_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row4_ne_0), ResizeBitCast(u8, row5_ne_0));
  auto row76_ne_0 = ConcatEven(u8, ResizeBitCast(u8, row6_ne_0), ResizeBitCast(u8, row7_ne_0));

  /* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
  alignas(16) uint8_t bm[] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
                              0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};

  auto bitmap_mask = Load(u8, bm);

  auto bitmap_rows_10 = And(row10_ne_0, bitmap_mask);
  auto bitmap_rows_32 = And(row32_ne_0, bitmap_mask);
  auto bitmap_rows_54 = And(row54_ne_0, bitmap_mask);
  auto bitmap_rows_76 = And(row76_ne_0, bitmap_mask);

  auto bitmap_rows_3210     = Padd(u8, bitmap_rows_32, bitmap_rows_10);
  auto bitmap_rows_7654     = Padd(u8, bitmap_rows_76, bitmap_rows_54);
  auto bitmap_rows_76543210 = Padd(u8, bitmap_rows_7654, bitmap_rows_3210);
  auto bitmap_all = Padd(u8_64, LowerHalf(bitmap_rows_76543210), UpperHalf(u8, bitmap_rows_76543210));
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = GetLane(BitCast(u64_64, bitmap_all));
  alignas(16) uint8_t bits[64];

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

  // EncodeDC(dp[0], bits[0], DC_cwd[c], DC_len[c], enc);
  enc.put_bits(DC_cwd[c][bits[0]], DC_len[c][bits[0]]);
  if (bits[0] != 0) {
    enc.put_bits(dp[0], bits[0]);
  }
  int count = 1;
  int run;
  while (bitmap != 0) {
    run = __builtin_clzll(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
      enc.put_bits(AC_cwd[c][0xF0], AC_len[c][0xF0]);
      run -= 16;
    }
    // EncodeAC(run, dp[count], bits[count], AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][(run << 4) + bits[count]], AC_len[c][(run << 4) + bits[count]]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][0x00], AC_len[c][0x00]);
  }
}
void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc) {
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
            make_zigzag_blk_simd(sp0, 0, prev_dc[0], enc);
          }
        }
        // Chroma, Cb
        make_zigzag_blk_simd(sp1, 1, prev_dc[1], enc);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        make_zigzag_blk_simd(sp2, 1, prev_dc[2], enc);
        sp2 += DCTSIZE2;
      }
    }
  } else {
    sp0 = in[0];
    for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        make_zigzag_blk_simd(sp0, 0, prev_dc[0], enc);
        sp0 += DCTSIZE2;
      }
    }
  }
}
}  // namespace jpegenc_hwy

#if 0

  #if defined(JPEG_USE_NEON)
    #include <arm_neon.h>
// clang-format off
// This table is borrowed from libjpeg-turbo
alignas(16) const uint8_t jsimd_huff_encode_one_block_consts[] = {
        0,   1,   2,   3,  16,  17,  32,  33,
        18,  19,   4,   5,   6,   7,  20,  21,
        34,  35,  48,  49, 255, 255,  50,  51,
        36,  37,  22,  23,   8,   9,  10,  11,
        255, 255,   6,   7,  20,  21,  34,  35,
        48,  49, 255, 255,  50,  51,  36,  37,
        54,  55,  40,  41,  26,  27,  12,  13,
        14,  15,  28,  29,  42,  43,  56,  57,
        6,   7,  20,  21,  34,  35,  48,  49,
        50,  51,  36,  37,  22,  23,   8,   9,
        26,  27,  12,  13, 255, 255,  14,  15,
        28,  29,  42,  43,  56,  57, 255, 255,
        52,  53,  54,  55,  40,  41,  26,  27,
        12,  13, 255, 255,  14,  15,  28,  29,
        26,  27,  40,  41,  42,  43,  28,  29,
        14,  15,  30,  31,  44,  45,  46,  47
};
  // clang-format on

  #else
    #include "zigzag_order.hpp"
static FORCE_INLINE void EncodeDC(int val, int16_t s, const unsigned int *Ctable, const int *Ltable,
                                  bitstream &enc) {
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0) {
    //    if (val < 0) {
    //      val -= 1;
    //    }
    val -= (val >> 31) & 1;
    enc.put_bits(val, s);
  }
}

static FORCE_INLINE void EncodeAC(int run, int val, int16_t s, const unsigned int *Ctable,
                                  const int *Ltable, bitstream &enc) {
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0) {
    val -= (val >> 31) & 1;
    enc.put_bits(val, s);
  }
}
  #endif

static void make_zigzag_blk(int16_t *sp, int c, int &prev_dc, bitstream &enc) {
  alignas(16) int16_t dp[64];
  int dc  = sp[0];
  sp[0]   = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc = dc;
  #if not defined(JPEG_USE_NEON)
  for (int i = 0; i < DCTSIZE2; ++i) {
    dp[i] = sp[scan[i]];
  }
  int run = 0;
  //  Branchless abs:
  //  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
  uint32_t uval = (dp[0] + (dp[0] >> 31)) ^ (dp[0] >> 31);
  int16_t s     = 0;
  int bound     = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
  EncodeDC(dp[0], s, DC_cwd[c], DC_len[c], enc);
  int ac;
  for (int i = 1; i < 64; ++i) {
    ac = dp[i];
    if (ac == 0) {
      run++;
    } else {
      while (run > 15) {
        // ZRL
        EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      s     = 0;
      bound = 1;
      uval  = (ac + (ac >> 31)) ^ (ac >> 31);
      while (uval >= bound) {
        bound += bound;
        s++;
      }
      EncodeAC(run, ac, s, AC_cwd[c], AC_len[c], enc);
      run = 0;
    }
  }
  if (run) {
    // EOB
    EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
  }
  #else
  // This code is borrowed from libjpeg-turbo
  const uint8x16x4_t idx_rows_0123 = vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 0 * DCTSIZE);
  const uint8x16x4_t idx_rows_4567 = vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 8 * DCTSIZE);

  const int8x16x4_t tbl_rows_0123 = vld1q_s8_x4((int8_t *)(sp + 0 * DCTSIZE));
  const int8x16x4_t tbl_rows_4567 = vld1q_s8_x4((int8_t *)(sp + 4 * DCTSIZE));

  /* Initialise extra lookup tables. */
  const int8x16x4_t tbl_rows_2345 = {
      {tbl_rows_0123.val[2], tbl_rows_0123.val[3], tbl_rows_4567.val[0], tbl_rows_4567.val[1]}};
  const int8x16x3_t tbl_rows_567 = {{tbl_rows_4567.val[1], tbl_rows_4567.val[2], tbl_rows_4567.val[3]}};

  /* Shuffle coefficients into zig-zag order. */
  int16x8_t row0 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[0]));
  int16x8_t row1 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[1]));
  int16x8_t row2 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_0123.val[2]));
  int16x8_t row3 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[3]));
  int16x8_t row4 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[0]));
  int16x8_t row5 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_4567.val[1]));
  int16x8_t row6 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[2]));
  int16x8_t row7 = vreinterpretq_s16_s8(vqtbl3q_s8(tbl_rows_567, idx_rows_4567.val[3]));

  /* Initialize AC coefficient lanes not reachable by lookup tables. */
  row1 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[0]), 0), row1, 2);
  row2 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 4), row2, 0);
  row2 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 0), row2, 5);
  row5 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 7), row5, 2);
  row5 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 3), row5, 7);
  row6 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[3]), 7), row6, 5);

  /* DCT block is now in zig-zag order; start Huffman encoding process. */

  /* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
   * means that the corresponding coefficient != 0.
   */
  uint16x8_t row0_ne_0 = vtstq_s16(row0, row0);
  uint16x8_t row1_ne_0 = vtstq_s16(row1, row1);
  uint16x8_t row2_ne_0 = vtstq_s16(row2, row2);
  uint16x8_t row3_ne_0 = vtstq_s16(row3, row3);
  uint16x8_t row4_ne_0 = vtstq_s16(row4, row4);
  uint16x8_t row5_ne_0 = vtstq_s16(row5, row5);
  uint16x8_t row6_ne_0 = vtstq_s16(row6, row6);
  uint16x8_t row7_ne_0 = vtstq_s16(row7, row7);

  uint8x16_t row10_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row1_ne_0), vreinterpretq_u8_u16(row0_ne_0));
  uint8x16_t row32_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row3_ne_0), vreinterpretq_u8_u16(row2_ne_0));
  uint8x16_t row54_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row5_ne_0), vreinterpretq_u8_u16(row4_ne_0));
  uint8x16_t row76_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row7_ne_0), vreinterpretq_u8_u16(row6_ne_0));

  /* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
  const uint8x16_t bitmap_mask = vreinterpretq_u8_u64(vdupq_n_u64(0x0102040810204080));

  uint8x16_t bitmap_rows_10 = vandq_u8(row10_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_32 = vandq_u8(row32_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_54 = vandq_u8(row54_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_76 = vandq_u8(row76_ne_0, bitmap_mask);

  uint8x16_t bitmap_rows_3210     = vpaddq_u8(bitmap_rows_32, bitmap_rows_10);
  uint8x16_t bitmap_rows_7654     = vpaddq_u8(bitmap_rows_76, bitmap_rows_54);
  uint8x16_t bitmap_rows_76543210 = vpaddq_u8(bitmap_rows_7654, bitmap_rows_3210);
  uint8x8_t bitmap_all = vpadd_u8(vget_low_u8(bitmap_rows_76543210), vget_high_u8(bitmap_rows_76543210));
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = vget_lane_u64(vreinterpret_u64_u8(bitmap_all), 0);
  uint8_t bits[64];

  int16x8_t abs_row0 = vabsq_s16(row0);
  int16x8_t abs_row1 = vabsq_s16(row1);
  int16x8_t abs_row2 = vabsq_s16(row2);
  int16x8_t abs_row3 = vabsq_s16(row3);
  int16x8_t abs_row4 = vabsq_s16(row4);
  int16x8_t abs_row5 = vabsq_s16(row5);
  int16x8_t abs_row6 = vabsq_s16(row6);
  int16x8_t abs_row7 = vabsq_s16(row7);

  int16x8_t row0_lz = vclzq_s16(abs_row0);
  int16x8_t row1_lz = vclzq_s16(abs_row1);
  int16x8_t row2_lz = vclzq_s16(abs_row2);
  int16x8_t row3_lz = vclzq_s16(abs_row3);
  int16x8_t row4_lz = vclzq_s16(abs_row4);
  int16x8_t row5_lz = vclzq_s16(abs_row5);
  int16x8_t row6_lz = vclzq_s16(abs_row6);
  int16x8_t row7_lz = vclzq_s16(abs_row7);

  /* Narrow leading zero count to 8 bits. */
  uint8x16_t row01_lz = vuzp1q_u8(vreinterpretq_u8_s16(row0_lz), vreinterpretq_u8_s16(row1_lz));
  uint8x16_t row23_lz = vuzp1q_u8(vreinterpretq_u8_s16(row2_lz), vreinterpretq_u8_s16(row3_lz));
  uint8x16_t row45_lz = vuzp1q_u8(vreinterpretq_u8_s16(row4_lz), vreinterpretq_u8_s16(row5_lz));
  uint8x16_t row67_lz = vuzp1q_u8(vreinterpretq_u8_s16(row6_lz), vreinterpretq_u8_s16(row7_lz));
  /* Compute nbits needed to specify magnitude of each coefficient. */
  uint8x16_t row01_nbits = vsubq_u8(vdupq_n_u8(16), row01_lz);
  uint8x16_t row23_nbits = vsubq_u8(vdupq_n_u8(16), row23_lz);
  uint8x16_t row45_nbits = vsubq_u8(vdupq_n_u8(16), row45_lz);
  uint8x16_t row67_nbits = vsubq_u8(vdupq_n_u8(16), row67_lz);
  /* Store nbits. */
  vst1q_u8(bits + 0 * DCTSIZE, row01_nbits);
  vst1q_u8(bits + 2 * DCTSIZE, row23_nbits);
  vst1q_u8(bits + 4 * DCTSIZE, row45_nbits);
  vst1q_u8(bits + 6 * DCTSIZE, row67_nbits);

  auto a               = vnegq_s16(row0_lz);
  auto b               = vcltzq_s16(row0);
  uint16x8_t row0_mask = vshlq_u16(vcltzq_s16(row0), vnegq_s16(row0_lz));
  uint16x8_t row1_mask = vshlq_u16(vcltzq_s16(row1), vnegq_s16(row1_lz));
  uint16x8_t row2_mask = vshlq_u16(vcltzq_s16(row2), vnegq_s16(row2_lz));
  uint16x8_t row3_mask = vshlq_u16(vcltzq_s16(row3), vnegq_s16(row3_lz));
  uint16x8_t row4_mask = vshlq_u16(vcltzq_s16(row4), vnegq_s16(row4_lz));
  uint16x8_t row5_mask = vshlq_u16(vcltzq_s16(row5), vnegq_s16(row5_lz));
  uint16x8_t row6_mask = vshlq_u16(vcltzq_s16(row6), vnegq_s16(row6_lz));
  uint16x8_t row7_mask = vshlq_u16(vcltzq_s16(row7), vnegq_s16(row7_lz));

  uint16x8_t row0_diff = veorq_u16(vreinterpretq_u16_s16(abs_row0), row0_mask);
  uint16x8_t row1_diff = veorq_u16(vreinterpretq_u16_s16(abs_row1), row1_mask);
  uint16x8_t row2_diff = veorq_u16(vreinterpretq_u16_s16(abs_row2), row2_mask);
  uint16x8_t row3_diff = veorq_u16(vreinterpretq_u16_s16(abs_row3), row3_mask);
  uint16x8_t row4_diff = veorq_u16(vreinterpretq_u16_s16(abs_row4), row4_mask);
  uint16x8_t row5_diff = veorq_u16(vreinterpretq_u16_s16(abs_row5), row5_mask);
  uint16x8_t row6_diff = veorq_u16(vreinterpretq_u16_s16(abs_row6), row6_mask);
  uint16x8_t row7_diff = veorq_u16(vreinterpretq_u16_s16(abs_row7), row7_mask);

  vst1q_s16(dp + 0 * DCTSIZE, row0_diff);
  vst1q_s16(dp + 1 * DCTSIZE, row1_diff);
  vst1q_s16(dp + 2 * DCTSIZE, row2_diff);
  vst1q_s16(dp + 3 * DCTSIZE, row3_diff);
  vst1q_s16(dp + 4 * DCTSIZE, row4_diff);
  vst1q_s16(dp + 5 * DCTSIZE, row5_diff);
  vst1q_s16(dp + 6 * DCTSIZE, row6_diff);
  vst1q_s16(dp + 7 * DCTSIZE, row7_diff);

  bitmap <<= 1;

  // EncodeDC(dp[0], bits[0], DC_cwd[c], DC_len[c], enc);
  enc.put_bits(DC_cwd[c][bits[0]], DC_len[c][bits[0]]);
  if (bits[0] != 0) {
    enc.put_bits(dp[0], bits[0]);
  }
  int count = 1;
  int run;
  while (bitmap != 0) {
    run = __builtin_clzll(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
      enc.put_bits(AC_cwd[c][0xF0], AC_len[c][0xF0]);
      run -= 16;
    }
    // EncodeAC(run, dp[count], bits[count], AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][(run << 4) + bits[count]], AC_len[c][(run << 4) + bits[count]]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][0x00], AC_len[c][0x00]);
  }
  #endif
}

void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc) {
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
            make_zigzag_blk(sp0, 0, prev_dc[0], enc);
          }
        }
        // Chroma, Cb
        make_zigzag_blk(sp1, 1, prev_dc[1], enc);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        make_zigzag_blk(sp2, 1, prev_dc[2], enc);
        sp2 += DCTSIZE2;
      }
    }
  } else {
    sp0 = in[0];
    for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        make_zigzag_blk(sp0, 0, prev_dc[0], enc);
        sp0 += DCTSIZE2;
      }
    }
  }
}
#endif