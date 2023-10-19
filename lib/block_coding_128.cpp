#include <cstdint>
#include <hwy/highway.h>
#include "constants.hpp"

// clang-format off
#if 1 && (HWY_TARGET == HWY_NEON) || (HWY_TARGET == HWY_NEON_WITHOUT_AES)
HWY_ALIGN constexpr uint8_t indices[] = {
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
#else
HWY_ALIGN constexpr int16_t indices[] = {
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
#endif
// clang-format on

hn::FixedTag<uint8_t, 8> u8_64;
hn::FixedTag<uint64_t, 1> u64_64;

#if 1 && (HWY_TARGET == HWY_NEON) || (HWY_TARGET == HWY_NEON_WITHOUT_AES)
const uint8x16x4_t idx_rows_0123 = vld1q_u8_x4(indices + 0 * DCTSIZE);
const uint8x16x4_t idx_rows_4567 = vld1q_u8_x4(indices + 8 * DCTSIZE);
const int8x16x4_t tbl_rows_0123  = vld1q_s8_x4((int8_t *)(sp + 0 * DCTSIZE));
const int8x16x4_t tbl_rows_4567  = vld1q_s8_x4((int8_t *)(sp + 4 * DCTSIZE));
/* Initialise extra lookup tables. */
const int8x16x4_t tbl_rows_2345 = {
    {tbl_rows_0123.val[2], tbl_rows_0123.val[3], tbl_rows_4567.val[0], tbl_rows_4567.val[1]}};
const int8x16x3_t tbl_rows_567 = {{tbl_rows_4567.val[1], tbl_rows_4567.val[2], tbl_rows_4567.val[3]}};

/* Shuffle coefficients into zig-zag order. */
int16x8_t rrow0 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[0]));
int16x8_t rrow1 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[1]));
int16x8_t rrow2 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_0123.val[2]));
int16x8_t rrow3 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[3]));
int16x8_t rrow4 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[0]));
int16x8_t rrow5 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_4567.val[1]));
int16x8_t rrow6 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[2]));
int16x8_t rrow7 = vreinterpretq_s16_s8(vqtbl3q_s8(tbl_rows_567, idx_rows_4567.val[3]));

/* Compute DC coefficient difference value (F.1.1.5.1). */
auto row0 = Undefined(s16);
auto row1 = Undefined(s16);
auto row2 = Undefined(s16);
auto row3 = Undefined(s16);
auto row4 = Undefined(s16);
auto row5 = Undefined(s16);
auto row6 = Undefined(s16);
auto row7 = Undefined(s16);
row0.raw  = rrow0;
row1.raw  = rrow1;
row2.raw  = rrow2;
row3.raw  = rrow3;
row4.raw  = rrow4;
row5.raw  = rrow5;
row6.raw  = rrow6;
row7.raw  = rrow7;

/* Initialize AC coefficient lanes not reachable by lookup tables. */
row1.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[0]), 0), row1.raw, 2);
row2.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 4), row2.raw, 0);
row2.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 0), row2.raw, 5);
row5.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 7), row5.raw, 2);
row5.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 3), row5.raw, 7);
row6.raw = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[3]), 7), row6.raw, 5);

#else
auto v0     = Load(s16, sp);
auto v1     = Load(s16, sp + 8);
auto v2     = Load(s16, sp + 16);
auto v3     = Load(s16, sp + 24);
auto v4     = Load(s16, sp + 32);
auto v5     = Load(s16, sp + 40);
auto v6     = Load(s16, sp + 48);
auto v7     = Load(s16, sp + 56);
auto row0   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[0 * 8]));
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

row0    = InsertLane(row0, 3, ExtractLane(v2, 0));
row0    = InsertLane(row0, 0, (int16_t)(sp[0] - prev_dc));
auto m5 = FirstN(s16, 5);
row1    = IfThenZeroElse(m5, row1);
row1_1  = IfThenElseZero(m5, row1_1);
row1    = Or(row1, row1_1);
row1    = InsertLane(row1, 2, ExtractLane(v4, 0));

auto m3 = FirstN(s16, 3);
row2    = IfThenZeroElse(m3, row2);
row2    = InsertLane(row2, 0, ExtractLane(v1, 4));
row2    = InsertLane(row2, 1, ExtractLane(v2, 3));
row2    = InsertLane(row2, 2, ExtractLane(v3, 2));
row2    = InsertLane(row2, 5, ExtractLane(v6, 0));

alignas(16) uint8_t mask34[8] = {0b00111100};
auto m34                      = LoadMaskBits(s16, mask34);
row3                          = IfThenZeroElse(m34, row3);
row3_1                        = IfThenElseZero(m34, row3_1);
row3                          = Or(row3, row3_1);

row4   = IfThenZeroElse(m34, row4);
row4_1 = IfThenElseZero(m34, row4_1);
row4   = Or(row4, row4_1);

row5 = IfThenZeroElse(Not(m5), row5);
row5 = InsertLane(row5, 2, ExtractLane(v1, 7));
row5 = InsertLane(row5, 5, ExtractLane(v4, 5));
row5 = InsertLane(row5, 6, ExtractLane(v5, 4));
row5 = InsertLane(row5, 7, ExtractLane(v6, 3));

row6   = IfThenZeroElse(m3, row6);
row6_1 = IfThenElseZero(m3, row6_1);
row6   = Or(row6, row6_1);
row6   = InsertLane(row6, 5, ExtractLane(v3, 7));

row7 = InsertLane(row7, 4, ExtractLane(v5, 7));
#endif

/* DCT block is now in zig-zag order; start Huffman encoding process. */

/* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
 * means that the corresponding coefficient != 0.
 */
auto zero       = Zero(s16);
auto row0_ne_0  = VecFromMask(s16, Eq(row0, zero));
auto row1_ne_0  = VecFromMask(s16, Eq(row1, zero));
auto row2_ne_0  = VecFromMask(s16, Eq(row2, zero));
auto row3_ne_0  = VecFromMask(s16, Eq(row3, zero));
auto row4_ne_0  = VecFromMask(s16, Eq(row4, zero));
auto row5_ne_0  = VecFromMask(s16, Eq(row5, zero));
auto row6_ne_0  = VecFromMask(s16, Eq(row6, zero));
auto row7_ne_0  = VecFromMask(s16, Eq(row7, zero));
auto row10_ne_0 = OrderedTruncate2To(u8, BitCast(u16, row1_ne_0), BitCast(u16, row0_ne_0));
auto row32_ne_0 = OrderedTruncate2To(u8, BitCast(u16, row3_ne_0), BitCast(u16, row2_ne_0));
auto row54_ne_0 = OrderedTruncate2To(u8, BitCast(u16, row5_ne_0), BitCast(u16, row4_ne_0));
auto row76_ne_0 = OrderedTruncate2To(u8, BitCast(u16, row7_ne_0), BitCast(u16, row6_ne_0));

/* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
HWY_ALIGN constexpr uint64_t bm[] = {0x0102040810204080, 0x0102040810204080};
auto bitmap_mask                  = BitCast(u8, Load(u64, bm));

auto bitmap_rows_10 = AndNot(row10_ne_0, bitmap_mask);
auto bitmap_rows_32 = AndNot(row32_ne_0, bitmap_mask);
auto bitmap_rows_54 = AndNot(row54_ne_0, bitmap_mask);
auto bitmap_rows_76 = AndNot(row76_ne_0, bitmap_mask);

auto bitmap_rows_3210     = PairwiseOrU8(bitmap_rows_32, bitmap_rows_10);
auto bitmap_rows_7654     = PairwiseOrU8(bitmap_rows_76, bitmap_rows_54);
auto bitmap_rows_76543210 = PairwiseOrU8(bitmap_rows_7654, bitmap_rows_3210);

auto t00        = DupEven(bitmap_rows_76543210);
auto t01        = DupOdd(bitmap_rows_76543210);
auto tt0        = Or(t00, t01);
auto bitmap_all = TruncateTo(u8_64, BitCast(u16, tt0));

/* Move bitmap to 64-bit scalar register. */
// bitmap = GetLane(BitCast(u64_64, bitmap_all));
Store(BitCast(u64_64, bitmap_all), u64_64, &bitmap);

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
auto row01_lz = OrderedTruncate2To(u8, BitCast(u16, row0_lz), BitCast(u16, row1_lz));
auto row23_lz = OrderedTruncate2To(u8, BitCast(u16, row2_lz), BitCast(u16, row3_lz));
auto row45_lz = OrderedTruncate2To(u8, BitCast(u16, row4_lz), BitCast(u16, row5_lz));
auto row67_lz = OrderedTruncate2To(u8, BitCast(u16, row6_lz), BitCast(u16, row7_lz));
// auto row01_lz = ConcatEven(u8, BitCast(u8, row1_lz), BitCast(u8, row0_lz));
// auto row23_lz = ConcatEven(u8, BitCast(u8, row3_lz), BitCast(u8, row2_lz));
// auto row45_lz = ConcatEven(u8, BitCast(u8, row5_lz), BitCast(u8, row4_lz));
// auto row67_lz = ConcatEven(u8, BitCast(u8, row7_lz), BitCast(u8, row6_lz));
/* Compute nbits needed to specify magnitude of each coefficient. */
const auto Val16 = Set(u8, 16);
auto row01_nbits = Sub(Val16, row01_lz);
auto row23_nbits = Sub(Val16, row23_lz);
auto row45_nbits = Sub(Val16, row45_lz);
auto row67_nbits = Sub(Val16, row67_lz);
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