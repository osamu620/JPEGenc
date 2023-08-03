// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "color.cpp"  // this file
#include <hwy/foreach_target.h>         // must come before highway.h

#include <hwy/highway.h>

#include <utility>

#include "color.hpp"
#include "ycctype.hpp"
#include "constants.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;
#if HWY_TARGET != HWY_SCALAR
HWY_ATTR void rgb2ycbcr(uint8_t *HWY_RESTRICT in, int width) {
  const hn::ScalableTag<uint8_t> d8;
  const hn::ScalableTag<uint16_t> d16;
  const hn::ScalableTag<int16_t> s16;

  HWY_ALIGN constexpr int16_t constants[] = {
      19595, 38470 - 32768, 7471, 11056, 21712, 0, 27440, 5328,
      19595, 38470 - 32768, 7471, 11056, 21712, 0, 27440, 5328,
      19595, 38470 - 32768, 7471, 11056, 21712, 0, 27440, 5328,
      19595, 38470 - 32768, 7471, 11056, 21712, 0, 27440, 5328
      };
  const auto coeffs                        = Load(s16, constants);
  auto v0                                  = Undefined(d8);
  auto v1                                  = Undefined(d8);
  auto v2                                  = Undefined(d8);
  auto scaled_128                          = Set(d16, 128 << 1);
  for (size_t i = width * LINES; i > 0; i -= Lanes(d8)) {
    LoadInterleaved3(d8, in, v0, v1, v2);
    auto r_l = PromoteTo(d16, LowerHalf(v0));
    auto g_l = PromoteTo(d16, LowerHalf(v1));
    auto b_l = PromoteTo(d16, LowerHalf(v2));
    auto r_h = PromoteUpperTo(d16, v0);
    auto g_h = PromoteUpperTo(d16, v1);
    auto b_h = PromoteUpperTo(d16, v2);
    // clang-format off
    auto yl  = BitCast(d16, MulFixedPoint15(BitCast(s16, r_l), hn::BroadcastLane<0>(coeffs)));
    yl       = Add(yl, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), hn::BroadcastLane<1>(coeffs))));
    yl       = Add(yl, BitCast(d16, MulFixedPoint15(BitCast(s16, b_l), hn::BroadcastLane<2>(coeffs))));
    //    yl       = ShiftRight<1>(Add(Add(yl, g_l), half));
    yl      = AverageRound(yl, g_l);
    auto yh = BitCast(d16, MulFixedPoint15(BitCast(s16, r_h), hn::BroadcastLane<0>(coeffs)));
    yh      = Add(yh, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), hn::BroadcastLane<1>(coeffs))));
    yh      = Add(yh, BitCast(d16, MulFixedPoint15(BitCast(s16, b_h), hn::BroadcastLane<2>(coeffs))));
    yh      = AverageRound(yh, g_h);

    auto cbl = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, r_l), hn::BroadcastLane<3>(coeffs))));
    cbl      = Sub(cbl, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), hn::BroadcastLane<4>(coeffs))));
    cbl      = AverageRound(b_l, cbl);
    auto cbh = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, r_h), hn::BroadcastLane<3>(coeffs))));
    cbh      = Sub(cbh, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), hn::BroadcastLane<4>(coeffs))));
    cbh      = AverageRound(b_h, cbh);

    auto crl = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), hn::BroadcastLane<6>(coeffs))));
    crl      = Sub(crl, BitCast(d16, MulFixedPoint15(BitCast(s16, b_l), hn::BroadcastLane<7>(coeffs))));
    crl      = AverageRound(r_l, crl);
    auto crh = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), hn::BroadcastLane<6>(coeffs))));
    crh      = Sub(crh, BitCast(d16, MulFixedPoint15(BitCast(s16, b_h), hn::BroadcastLane<7>(coeffs))));
    crh      = AverageRound(r_h, crh);
    // clang-format on
    v0 = OrderedTruncate2To(d8, yl, yh);
    v1 = OrderedDemote2To(d8, cbl, cbh);
    v2 = OrderedDemote2To(d8, crl, crh);
    StoreInterleaved3(v0, v1, v2, d8, in);

    in += 3 * Lanes(d8);
  }
}

HWY_ATTR void subsample_core(uint8_t *HWY_RESTRICT in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc      = (YCCtype == YCC::GRAY) ? 1 : 3;
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;

  int shift = 0;
  int bound = 1;
  while ((scale_x * scale_y) > bound) {
    bound += bound;
    shift++;
  }
  int half = 0;
  if (shift) {
    half = 1 << (shift - 1);
  }
  size_t pos        = 0;
  size_t pos_Chroma = 0;
  //  HWY_CAPPED(uint8_t, 16) u8;
  //  HWY_CAPPED(int16_t, 8) s16;
  hn::FixedTag<uint8_t, 16> u8;
  hn::FixedTag<uint8_t, 8> h8;
  hn::FixedTag<int16_t, 8> s16;
  auto c128  = Set(u8, 128);
  auto vhalf = Set(s16, half);
  switch (YCCtype) {
    case YCC::GRAY:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp = in + nc * i * width + nc * j;
          auto v0 = Load(u8, sp + 0 * width * nc);
          auto v1 = Load(u8, sp + 1 * width * nc);
          auto v2 = Load(u8, sp + 2 * width * nc);
          auto v3 = Load(u8, sp + 3 * width * nc);
          auto v4 = Load(u8, sp + 4 * width * nc);
          auto v5 = Load(u8, sp + 5 * width * nc);
          auto v6 = Load(u8, sp + 6 * width * nc);
          auto v7 = Load(u8, sp + 7 * width * nc);

          Store(Sub(PromoteTo(s16, LowerHalf(v0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 7);
          pos += 64;
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 7);
          pos += 64;
        }
      }
      break;
    case YCC::YUV444:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp = in + nc * i * width + nc * j;
          // clang-format off
          auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
          auto v1_0 = Undefined(u8); auto v1_1 = Undefined(u8); auto v1_2 = Undefined(u8);
          auto v2_0 = Undefined(u8); auto v2_1 = Undefined(u8); auto v2_2 = Undefined(u8);
          auto v3_0 = Undefined(u8); auto v3_1 = Undefined(u8); auto v3_2 = Undefined(u8);
          auto v4_0 = Undefined(u8); auto v4_1 = Undefined(u8); auto v4_2 = Undefined(u8);
          auto v5_0 = Undefined(u8); auto v5_1 = Undefined(u8); auto v5_2 = Undefined(u8);
          auto v6_0 = Undefined(u8); auto v6_1 = Undefined(u8); auto v6_2 = Undefined(u8);
          auto v7_0 = Undefined(u8); auto v7_1 = Undefined(u8); auto v7_2 = Undefined(u8);
          // clang-format on

          LoadInterleaved3(u8, sp + 0 * width * nc, v0_0, v0_1, v0_2);
          LoadInterleaved3(u8, sp + 1 * width * nc, v1_0, v1_1, v1_2);
          LoadInterleaved3(u8, sp + 2 * width * nc, v2_0, v2_1, v2_2);
          LoadInterleaved3(u8, sp + 3 * width * nc, v3_0, v3_1, v3_2);
          LoadInterleaved3(u8, sp + 4 * width * nc, v4_0, v4_1, v4_2);
          LoadInterleaved3(u8, sp + 5 * width * nc, v5_0, v5_1, v5_2);
          LoadInterleaved3(u8, sp + 6 * width * nc, v6_0, v6_1, v6_2);
          LoadInterleaved3(u8, sp + 7 * width * nc, v7_0, v7_1, v7_2);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 15);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[1] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_1)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[1] + pos + 8 * 15);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[2] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_2)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[2] + pos + 8 * 15);
          pos += 128;
        }
      }
      break;
    case YCC::YUV422:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp = in + nc * i * width + nc * j;

          // clang-format off
          auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
          auto v1_0 = Undefined(u8); auto v1_1 = Undefined(u8); auto v1_2 = Undefined(u8);
          auto v2_0 = Undefined(u8); auto v2_1 = Undefined(u8); auto v2_2 = Undefined(u8);
          auto v3_0 = Undefined(u8); auto v3_1 = Undefined(u8); auto v3_2 = Undefined(u8);
          auto v4_0 = Undefined(u8); auto v4_1 = Undefined(u8); auto v4_2 = Undefined(u8);
          auto v5_0 = Undefined(u8); auto v5_1 = Undefined(u8); auto v5_2 = Undefined(u8);
          auto v6_0 = Undefined(u8); auto v6_1 = Undefined(u8); auto v6_2 = Undefined(u8);
          auto v7_0 = Undefined(u8); auto v7_1 = Undefined(u8); auto v7_2 = Undefined(u8);
          // clang-format on

          LoadInterleaved3(u8, sp + 0 * width * nc, v0_0, v0_1, v0_2);
          LoadInterleaved3(u8, sp + 1 * width * nc, v1_0, v1_1, v1_2);
          LoadInterleaved3(u8, sp + 2 * width * nc, v2_0, v2_1, v2_2);
          LoadInterleaved3(u8, sp + 3 * width * nc, v3_0, v3_1, v3_2);
          LoadInterleaved3(u8, sp + 4 * width * nc, v4_0, v4_1, v4_2);
          LoadInterleaved3(u8, sp + 5 * width * nc, v5_0, v5_1, v5_2);
          LoadInterleaved3(u8, sp + 6 * width * nc, v6_0, v6_1, v6_2);
          LoadInterleaved3(u8, sp + 7 * width * nc, v7_0, v7_1, v7_2);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 15);

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01)), vhalf)), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11)), vhalf)), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb20), BitCast(s16, cb21)), vhalf)), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb30), BitCast(s16, cb31)), vhalf)), s16, out[1] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb40), BitCast(s16, cb41)), vhalf)), s16, out[1] + pos_Chroma + 8 * 4);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb50), BitCast(s16, cb51)), vhalf)), s16, out[1] + pos_Chroma + 8 * 5);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb60), BitCast(s16, cb61)), vhalf)), s16, out[1] + pos_Chroma + 8 * 6);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb70), BitCast(s16, cb71)), vhalf)), s16, out[1] + pos_Chroma + 8 * 7);
          // clang-format on

          cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<1>(Add(Padd(s16, cb00, cb01), vhalf)), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11)), vhalf)), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb20), BitCast(s16, cb21)), vhalf)), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb30), BitCast(s16, cb31)), vhalf)), s16, out[2] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb40), BitCast(s16, cb41)), vhalf)), s16, out[2] + pos_Chroma + 8 * 4);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb50), BitCast(s16, cb51)), vhalf)), s16, out[2] + pos_Chroma + 8 * 5);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb60), BitCast(s16, cb61)), vhalf)), s16, out[2] + pos_Chroma + 8 * 6);
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb70), BitCast(s16, cb71)), vhalf)), s16, out[2] + pos_Chroma + 8 * 7);
          // clang-format on
          pos += 128;
          pos_Chroma += 64;
        }
      }
      break;
    case YCC::YUV440:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp    = in + nc * i * width + nc * j;
          pos_Chroma = j * 8 + i * 4;
          // clang-format off
          auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
          auto v1_0 = Undefined(u8); auto v1_1 = Undefined(u8); auto v1_2 = Undefined(u8);
          auto v2_0 = Undefined(u8); auto v2_1 = Undefined(u8); auto v2_2 = Undefined(u8);
          auto v3_0 = Undefined(u8); auto v3_1 = Undefined(u8); auto v3_2 = Undefined(u8);
          auto v4_0 = Undefined(u8); auto v4_1 = Undefined(u8); auto v4_2 = Undefined(u8);
          auto v5_0 = Undefined(u8); auto v5_1 = Undefined(u8); auto v5_2 = Undefined(u8);
          auto v6_0 = Undefined(u8); auto v6_1 = Undefined(u8); auto v6_2 = Undefined(u8);
          auto v7_0 = Undefined(u8); auto v7_1 = Undefined(u8); auto v7_2 = Undefined(u8);
          // clang-format on

          LoadInterleaved3(u8, sp + 0 * width * nc, v0_0, v0_1, v0_2);
          LoadInterleaved3(u8, sp + 1 * width * nc, v1_0, v1_1, v1_2);
          LoadInterleaved3(u8, sp + 2 * width * nc, v2_0, v2_1, v2_2);
          LoadInterleaved3(u8, sp + 3 * width * nc, v3_0, v3_1, v3_2);
          LoadInterleaved3(u8, sp + 4 * width * nc, v4_0, v4_1, v4_2);
          LoadInterleaved3(u8, sp + 5 * width * nc, v5_0, v5_1, v5_2);
          LoadInterleaved3(u8, sp + 6 * width * nc, v6_0, v6_1, v6_2);
          LoadInterleaved3(u8, sp + 7 * width * nc, v7_0, v7_1, v7_2);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 15);

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

          Store(hn::ShiftRight<1>(Add(Add(cb00, cb10), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(Add(cb20, cb30), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(Add(cb40, cb50), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(Add(cb60, cb70), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(Add(cb01, cb11), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 8);
          Store(hn::ShiftRight<1>(Add(Add(cb21, cb31), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 9);
          Store(hn::ShiftRight<1>(Add(Add(cb41, cb51), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 10);
          Store(hn::ShiftRight<1>(Add(Add(cb61, cb71), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 11);

          cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

          Store(hn::ShiftRight<1>(Add(Add(cb00, cb10), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(Add(cb20, cb30), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(Add(cb40, cb50), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(Add(cb60, cb70), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(Add(cb01, cb11), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 8);
          Store(hn::ShiftRight<1>(Add(Add(cb21, cb31), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 9);
          Store(hn::ShiftRight<1>(Add(Add(cb41, cb51), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 10);
          Store(hn::ShiftRight<1>(Add(Add(cb61, cb71), Set(s16, 1))), s16, out[2] + pos_Chroma + 8 * 11);

          pos += 128;
        }
      }
      break;
    case YCC::YUV420:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp    = in + nc * i * width + nc * j;
          pos_Chroma = j * 4 + i * 4;
          // clang-format off
          auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
          auto v1_0 = Undefined(u8); auto v1_1 = Undefined(u8); auto v1_2 = Undefined(u8);
          auto v2_0 = Undefined(u8); auto v2_1 = Undefined(u8); auto v2_2 = Undefined(u8);
          auto v3_0 = Undefined(u8); auto v3_1 = Undefined(u8); auto v3_2 = Undefined(u8);
          auto v4_0 = Undefined(u8); auto v4_1 = Undefined(u8); auto v4_2 = Undefined(u8);
          auto v5_0 = Undefined(u8); auto v5_1 = Undefined(u8); auto v5_2 = Undefined(u8);
          auto v6_0 = Undefined(u8); auto v6_1 = Undefined(u8); auto v6_2 = Undefined(u8);
          auto v7_0 = Undefined(u8); auto v7_1 = Undefined(u8); auto v7_2 = Undefined(u8);
          // clang-format on

          LoadInterleaved3(u8, sp + 0 * width * nc, v0_0, v0_1, v0_2);
          LoadInterleaved3(u8, sp + 1 * width * nc, v1_0, v1_1, v1_2);
          LoadInterleaved3(u8, sp + 2 * width * nc, v2_0, v2_1, v2_2);
          LoadInterleaved3(u8, sp + 3 * width * nc, v3_0, v3_1, v3_2);
          LoadInterleaved3(u8, sp + 4 * width * nc, v4_0, v4_1, v4_2);
          LoadInterleaved3(u8, sp + 5 * width * nc, v5_0, v5_1, v5_2);
          LoadInterleaved3(u8, sp + 6 * width * nc, v6_0, v6_1, v6_2);
          LoadInterleaved3(u8, sp + 7 * width * nc, v7_0, v7_1, v7_2);

          Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, LowerHalf(v2_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, LowerHalf(v3_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, LowerHalf(v4_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, LowerHalf(v5_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, LowerHalf(v6_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, LowerHalf(v7_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                out[0] + pos + 8 * 7);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v2_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v3_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v4_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v5_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v6_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(h8, v7_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                out[0] + pos + 8 * 15);
          // clang-format on

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb00, cb10)), BitCast(s16, Add(cb01, cb11))), vhalf)), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb20, cb30)), BitCast(s16, Add(cb21, cb31))), vhalf)), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb40, cb50)), BitCast(s16, Add(cb41, cb51))), vhalf)), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb60, cb70)), BitCast(s16, Add(cb61, cb71))), vhalf)), s16, out[1] + pos_Chroma + 8 * 3);
          // clang-format on

          cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb21 = Sub(PromoteTo(s16, UpperHalf(h8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb31 = Sub(PromoteTo(s16, UpperHalf(h8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb41 = Sub(PromoteTo(s16, UpperHalf(h8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb51 = Sub(PromoteTo(s16, UpperHalf(h8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb61 = Sub(PromoteTo(s16, UpperHalf(h8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          cb71 = Sub(PromoteTo(s16, UpperHalf(h8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb00, cb10)), BitCast(s16, Add(cb01, cb11))), vhalf)), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb20, cb30)), BitCast(s16, Add(cb21, cb31))), vhalf)), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb40, cb50)), BitCast(s16, Add(cb41, cb51))), vhalf)), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb60, cb70)), BitCast(s16, Add(cb61, cb71))), vhalf)), s16, out[2] + pos_Chroma + 8 * 3);
          // clang-format on

          pos += 128;
        }
      }
      break;
    case YCC::YUV411:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8) * 2) {
          auto sp  = in + nc * i * width + nc * j;
          size_t p = 0;
          for (int y = 0; y < DCTSIZE; ++y) {
            // clang-format off
                            auto v0_0 = Undefined(u8);
                            auto v0_1 = Undefined(u8);
                            auto v0_2 = Undefined(u8);
                            auto v1_0 = Undefined(u8);
                            auto v1_1 = Undefined(u8);
                            auto v1_2 = Undefined(u8);
            // clang-format on

            LoadInterleaved3(u8, sp + y * width * nc, v0_0, v0_1, v0_2);
            LoadInterleaved3(u8, sp + y * width * nc + nc * Lanes(u8), v1_0, v1_1, v1_2);

            // Y
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p);
            Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 64);
            Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 128);
            Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 192);

            // Cb
            auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, LowerHalf(c128)));

            auto t0   = Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01));
            auto t1   = Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11));
            auto tb00 = Padd(s16, t0, t1);
            Store(hn::ShiftRight<2>(Add(Padd(s16, t0, t1), vhalf)), s16, out[1] + pos_Chroma + p);

            // Cr
            cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
            cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
            cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
            cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, LowerHalf(c128)));

            t0   = Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01));
            t1   = Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11));
            tb00 = Padd(s16, t0, t1);
            Store(hn::ShiftRight<2>(Add(Padd(s16, t0, t1), vhalf)), s16, out[2] + pos_Chroma + p);
            p += 8;
          }
          pos += 256;
          pos_Chroma += 64;
        }
      }
      break;
    case YCC::YUV410:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8) * 2) {
          auto sp  = in + nc * i * width + nc * j;
          size_t p = 0, pc = 0;
          pos_Chroma = j * 2 + i * 4;
          auto cb    = Undefined(s16);
          auto cr    = Undefined(s16);
          for (int y = 0; y < DCTSIZE; ++y) {
            // clang-format off
            auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
            auto v1_0 = Undefined(u8); auto v1_1 = Undefined(u8); auto v1_2 = Undefined(u8);
            // clang-format on

            LoadInterleaved3(u8, sp + y * width * nc, v0_0, v0_1, v0_2);
            LoadInterleaved3(u8, sp + y * width * nc + nc * Lanes(u8), v1_0, v1_1, v1_2);

            // Y
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p);
            Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 64);
            Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 128);
            Store(Sub(PromoteTo(s16, UpperHalf(h8, v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 192);
            // Cb
            auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_1)), PromoteTo(s16, LowerHalf(c128)));

            auto tb0 = Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01));
            auto tb1 = Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11));

            // Cr
            auto cr00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr01 = Sub(PromoteTo(s16, UpperHalf(h8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr11 = Sub(PromoteTo(s16, UpperHalf(h8, v1_2)), PromoteTo(s16, LowerHalf(c128)));

            auto tr0 = Padd(s16, BitCast(s16, cr00), BitCast(s16, cr01));
            auto tr1 = Padd(s16, BitCast(s16, cr10), BitCast(s16, cr11));

            if (y % 2 == 0) {
              cb = Padd(s16, tb0, tb1);
              cr = Padd(s16, tr0, tr1);
            } else {
              cb = hn::ShiftRight<3>(Add(Add(cb, Padd(s16, tb0, tb1)), vhalf));
              cr = hn::ShiftRight<3>(Add(Add(cr, Padd(s16, tr0, tr1)), vhalf));
              Store(cb, s16, out[1] + pos_Chroma + pc);
              Store(cr, s16, out[2] + pos_Chroma + pc);
              pc += 8;
            }
            p += 8;
          }
          pos += 256;
        }
      }
      break;
    case YCC::GRAY2:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            // clang-format off
            auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
            // clang-format on

            LoadInterleaved3(u8, sp + y * width * nc, v0_0, v0_1, v0_2);
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + y * DCTSIZE);
            Store(Sub(PromoteTo(s16, UpperHalf(h8, v0_0)), PromoteTo(s16, UpperHalf(h8, c128))), s16,
                  out[0] + pos + 64 + y * DCTSIZE);
          }
          pos += 128;
        }
      }
      break;
    default:  // Shall not reach here
      break;
  }
}
#else
HWY_ATTR void rgb2ycbcr(uint8_t *HWY_RESTRICT in, int width) {
  uint8_t *I0 = in, *I1 = in + 1, *I2 = in + 2;
  constexpr int32_t c00   = 9798;    // 0.299 * 2^15
  constexpr int32_t c01   = 19235;   // 0.587 * 2^15
  constexpr int32_t c02   = 3736;    // 0.114 * 2^15
  constexpr int32_t c10   = -5528;   // -0.1687 * 2^15
  constexpr int32_t c11   = -10856;  // -0.3313 * 2^15
  constexpr int32_t c12   = 16384;   // 0.5 * 2^15
  constexpr int32_t c20   = 16384;   // 0.5 * 2^15
  constexpr int32_t c21   = -13720;  // -0.4187 * 2^15
  constexpr int32_t c22   = -2664;   // -0.0813 * 2^15
  constexpr int32_t shift = 15;
  constexpr int32_t half  = 1 << (shift - 1);
  int32_t Y, Cb, Cr;
  for (int i = 0; i < width * 3 * LINES; i += 3) {
    Y     = ((c00 * I0[0] + c01 * I1[0] + c02 * I2[0] + half) >> shift);
    Cb    = (c10 * I0[0] + c11 * I1[0] + c12 * I2[0] + half) >> shift;
    Cr    = (c20 * I0[0] + c21 * I1[0] + c22 * I2[0] + half) >> shift;
    I0[0] = static_cast<uint8_t>(Y);
    I1[0] = static_cast<uint8_t>(Cb + 128);
    I2[0] = static_cast<uint8_t>(Cr + 128);
    I0 += 3;
    I1 += 3;
    I2 += 3;
  }
}

HWY_ATTR void subsample_core(uint8_t *HWY_RESTRICT in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc      = (YCCtype == YCC::GRAY) ? 1 : 3;
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;

  int shift = 0;
  int bound = 1;
  while ((scale_x * scale_y) > bound) {
    bound += bound;
    shift++;
  }
  int half = 0;
  if (shift) {
    half = 1 << (shift - 1);
  }
  size_t pos = 0;
  // Luma, Y, just copying
  for (int i = 0; i < LINES; i += DCTSIZE) {
    for (int j = 0; j < width; j += DCTSIZE) {
      auto sp = in + nc * i * width + nc * j;
      for (int y = 0; y < DCTSIZE; ++y) {
        for (int x = 0; x < DCTSIZE; ++x) {
          out[0][pos] = sp[y * width * nc + nc * x] - 128;
          pos++;
        }
      }
    }
  }
  // Chroma, Cb and Cr
  for (int c = 1; c < nc; ++c) {
    pos = 0;
    for (int i = 0; i < LINES; i += DCTSIZE * scale_y) {
      for (int j = 0; j < width; j += DCTSIZE * scale_x) {
        auto sp = in + nc * i * width + nc * j + c;
        for (int y = 0; y < DCTSIZE * scale_y; y += scale_y) {
          for (int x = 0; x < DCTSIZE * scale_x; x += scale_x) {
            int ave = 0;
            for (int p = 0; p < scale_y; ++p) {
              for (int q = 0; q < scale_x; ++q) {
                ave += sp[(y + p) * width * nc + nc * (x + q)];
              }
            }
            ave += half;
            ave >>= shift;
            out[c][pos] = static_cast<int16_t>(ave - 128);
            pos++;
          }
        }
      }
    }
  }
}
#endif
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(rgb2ycbcr);
HWY_EXPORT(subsample_core);

void rgb2ycbcr(uint8_t *HWY_RESTRICT in, int width) { HWY_DYNAMIC_DISPATCH(rgb2ycbcr)(in, width); }
void subsample(uint8_t *HWY_RESTRICT in, std::vector<int16_t *> out, int width, int YCCtype) {
  HWY_DYNAMIC_DISPATCH(subsample_core)(in, std::move(out), width, YCCtype);
};
}  // namespace jpegenc_hwy
#endif