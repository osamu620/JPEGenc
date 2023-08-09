#include <cstdint>
#include <hwy/highway.h>

uint64_t bitmap;
HWY_ALIGN int16_t dp[64];
HWY_ALIGN uint8_t bits[64];

// clang-format off
HWY_ALIGN constexpr int16_t indices[64] = {
        0,  1,  8,  16, 9,  2,  3,  10,
        17, 24, 32, 25, 18, 11, 4,  5,
        12, 19, 26, 33, 40, 48, 41, 34,
        27, 20, 13, 6,  7,  14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36,
        29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46,
        53, 60, 61, 54, 47, 55, 62, 63
};
// clang-format on

using namespace hn;

const ScalableTag<int16_t> s16;
const ScalableTag<uint16_t> u16;
const ScalableTag<uint8_t> u8;
const ScalableTag<uint64_t> u64;

auto v0 = Load(s16, sp);
auto v1 = Load(s16, sp + 32);

auto row0123 = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[0 * 32]));
auto row4567 = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[1 * 32]));

/* DCT block is now in zig-zag order; start Huffman encoding process. */

/* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
 * means that the corresponding coefficient != 0.
 */
auto zero             = Zero(s16);
auto row0123_ne_0     = VecFromMask(s16, Eq(row0123, zero));
auto row4567_ne_0     = VecFromMask(s16, Eq(row4567, zero));
auto row76543210_ne_0 = ConcatEven(u8, BitCast(u8, row4567_ne_0), BitCast(u8, row0123_ne_0));

// clang-format off
HWY_ALIGN constexpr uint8_t bm[] = {
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};
// clang-format on

auto bitmap_mask          = Load(u8, bm);
auto bitmap_rows_76543210 = AndNot(row76543210_ne_0, bitmap_mask);
auto a0                   = SumsOf8(bitmap_rows_76543210);
/* Move bitmap to 64-bit scalar register. */
HWY_ALIGN uint64_t shift[8] = {1UL << 56, 1UL << 48, 1UL << 40, 1UL << 32,
                               1UL << 24, 1UL << 16, 1UL << 8,  1UL};
auto vs                     = Load(u64, shift);
a0                          = Mul(a0, vs);
bitmap                      = GetLane(SumOfLanes(u64, a0));

auto abs_row0123 = Abs(row0123);
auto abs_row4567 = Abs(row4567);

auto row0123_lz = LeadingZeroCount(abs_row0123);
auto row4567_lz = LeadingZeroCount(abs_row4567);
/* Narrow leading zero count to 8 bits. */
auto row01234567_lz = ConcatEven(u8, BitCast(u8, row4567_lz), BitCast(u8, row0123_lz));
/* Compute nbits needed to specify magnitude of each coefficient. */
auto row01234567_nbits = Sub(Set(u8, 16), row01234567_lz);
/* Store nbits. */
Store(row01234567_nbits, u8, bits);

auto row0123_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row0123, zero))), BitCast(u16, row0123_lz));
auto row4567_mask = Shr(BitCast(u16, VecFromMask(s16, Lt(row4567, zero))), BitCast(u16, row4567_lz));

auto row0123_diff = Xor(BitCast(u16, abs_row0123), row0123_mask);
auto row4567_diff = Xor(BitCast(u16, abs_row4567), row4567_mask);

Store(BitCast(s16, row0123_diff), s16, dp + 0 * DCTSIZE);
Store(BitCast(s16, row4567_diff), s16, dp + 4 * DCTSIZE);

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
