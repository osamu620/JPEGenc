#include <cstdint>
#include <hwy/highway.h>

// clang-format off
HWY_ALIGN constexpr int16_t indices[] = {
        0,  1,  8, 16,  9,  2,  3, 10,
        17, 24,  0, 25, 18, 11,  4,  5,
        12, 19, 26,  0,  0,  0,  0,  0,
        27, 20, 13,  6,  7, 14, 21, 28,
        3, 10, 17, 24, 25, 18, 11,  4,
        0,  0,  0,  0,  0,  5, 12, 19,
        26, 27, 20, 13,  6,  0,  7, 14,
        21, 28, 29, 22, 15, 23, 30, 31,
        0,  0,  0,  1,  8, 16,  9,  2,
        0,  0,  0,  0,  0,  0,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        29, 22, 15, 23, 30,  0,  0,  0
};
// clang-format on

auto v0 = Load(s16, sp);
auto v1 = Load(s16, sp + 16);
auto v2 = Load(s16, sp + 32);
auto v3 = Load(s16, sp + 48);

auto row01   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[0 * 16]));
auto row23   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[1 * 16]));
auto row45   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[2 * 16]));
auto row67   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[3 * 16]));
auto row23_1 = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[4 * 16]));
auto row45_1 = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[5 * 16]));

HWY_ALIGN int16_t m[32] = {
    -1, -1, -1, 0,  0,  0,  0,  0,  -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, 0,  0,  0,  0,  0,  -1, -1, -1,
};
auto maskv1 = Load(s16, m);
auto maskv2 = Load(s16, m + 16);
row23       = IfThenElseZero(MaskFromVec(maskv1), row23);
row45       = IfThenElseZero(MaskFromVec(maskv2), row45);
row23_1     = IfThenZeroElse(MaskFromVec(maskv1), row23_1);
row45_1     = IfThenZeroElse(MaskFromVec(maskv2), row45_1);
row23       = Or(row23, row23_1);
row45       = Or(row45, row45_1);
row01       = InsertLane(row01, 10, ExtractLane(v2, 0));
row67       = InsertLane(row67, 5, ExtractLane(v1, 15));

/* DCT block is now in zig-zag order; start Huffman encoding process. */

/* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
 * means that the corresponding coefficient != 0.
 */
auto zero         = Zero(s16);
auto row01_ne_0   = VecFromMask(s16, Eq(row01, zero));
auto row23_ne_0   = VecFromMask(s16, Eq(row23, zero));
auto row45_ne_0   = VecFromMask(s16, Eq(row45, zero));
auto row67_ne_0   = VecFromMask(s16, Eq(row67, zero));
auto row3210_ne_0 = ConcatEven(u8, BitCast(u8, row23_ne_0), BitCast(u8, row01_ne_0));
auto row7654_ne_0 = ConcatEven(u8, BitCast(u8, row67_ne_0), BitCast(u8, row45_ne_0));

/* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
HWY_ALIGN constexpr uint64_t bm[] = {0x0102040810204080, 0x0102040810204080, 0x0102040810204080,
                                     0x0102040810204080};
auto bitmap_mask                  = BitCast(u8, Load(u64, bm));

auto bitmap_rows_3210 = AndNot(row3210_ne_0, bitmap_mask);
auto bitmap_rows_7654 = AndNot(row7654_ne_0, bitmap_mask);
auto a0               = SumsOf8(bitmap_rows_3210);
auto a1               = SumsOf8(bitmap_rows_7654);
/* Move bitmap to 64-bit scalar register. */
HWY_ALIGN uint64_t shift[4] = {24, 16, 8, 0};
const auto vs               = Load(u64, shift);
a0                          = Shl(a0, vs);
a1                          = Shl(a1, vs);
bitmap                      = (GetLane(SumOfLanes(u64, a0)) << 32) + GetLane(SumOfLanes(u64, a1));

auto abs_row01 = Abs(row01);
auto abs_row23 = Abs(row23);
auto abs_row45 = Abs(row45);
auto abs_row67 = Abs(row67);

auto row01_lz = LeadingZeroCount(abs_row01);
auto row23_lz = LeadingZeroCount(abs_row23);
auto row45_lz = LeadingZeroCount(abs_row45);
auto row67_lz = LeadingZeroCount(abs_row67);
/* Narrow leading zero count to 8 bits. */
auto row0123_lz = ConcatEven(u8, BitCast(u8, row23_lz), BitCast(u8, row01_lz));
auto row4567_lz = ConcatEven(u8, BitCast(u8, row67_lz), BitCast(u8, row45_lz));
/* Compute nbits needed to specify magnitude of each coefficient. */
auto row0123_nbits = Sub(Set(u8, 16), row0123_lz);
auto row4567_nbits = Sub(Set(u8, 16), row4567_lz);
/* Store nbits. */
Store(row0123_nbits, u8, bits + 0 * DCTSIZE);
Store(row4567_nbits, u8, bits + 4 * DCTSIZE);

auto row01_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row01, zero))), BitCast(u16, row01_lz));
auto row23_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row23, zero))), BitCast(u16, row23_lz));
auto row45_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row45, zero))), BitCast(u16, row45_lz));
auto row67_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row67, zero))), BitCast(u16, row67_lz));

auto row01_diff = Xor(BitCast(u16, abs_row01), row01_mask);
auto row23_diff = Xor(BitCast(u16, abs_row23), row23_mask);
auto row45_diff = Xor(BitCast(u16, abs_row45), row45_mask);
auto row67_diff = Xor(BitCast(u16, abs_row67), row67_mask);

Store(BitCast(s16, row01_diff), s16, dp + 0 * DCTSIZE);
Store(BitCast(s16, row23_diff), s16, dp + 2 * DCTSIZE);
Store(BitCast(s16, row45_diff), s16, dp + 4 * DCTSIZE);
Store(BitCast(s16, row67_diff), s16, dp + 6 * DCTSIZE);