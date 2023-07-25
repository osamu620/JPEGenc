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
HWY_ATTR void rgb2ycbcr_simd(uint8_t *HWY_RESTRICT in, int width) {
  const hn::ScalableTag<uint8_t> d8;
  const hn::ScalableTag<uint16_t> d16;
  const hn::ScalableTag<int16_t> s16;

  alignas(32) constexpr uint16_t constants[8] = {19595, 38470 - 32768, 7471,  11056,
                                                 21712, 32768,         27440, 5328};

  auto v0                      = Undefined(d8);
  auto v1                      = Undefined(d8);
  auto v2                      = Undefined(d8);
  const size_t N               = Lanes(d8);
  auto coeff0                  = Set(d16, constants[0]);
  auto coeff1                  = Set(d16, constants[1]);
  auto coeff2                  = Set(d16, constants[2]);
  auto coeff3                  = Set(d16, constants[3]);
  auto coeff4                  = Set(d16, constants[4]);
  [[maybe_unused]] auto coeff5 = Set(d16, constants[5]);
  auto coeff6                  = Set(d16, constants[6]);
  auto coeff7                  = Set(d16, constants[7]);
  auto scaled_128              = Set(d16, 128 << 1);
  for (size_t i = width * LINES; i > 0; i -= N) {
    LoadInterleaved3(d8, in, v0, v1, v2);
    auto r_l = PromoteTo(d16, LowerHalf(v0));
    auto g_l = PromoteTo(d16, LowerHalf(v1));
    auto b_l = PromoteTo(d16, LowerHalf(v2));
    auto r_h = PromoteTo(d16, UpperHalf(d8, v0));
    auto g_h = PromoteTo(d16, UpperHalf(d8, v1));
    auto b_h = PromoteTo(d16, UpperHalf(d8, v2));
    auto yl  = BitCast(d16, MulFixedPoint15(BitCast(s16, r_l), BitCast(s16, coeff0)));
    yl       = Add(yl, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), BitCast(s16, coeff1))));
    yl       = Add(yl, BitCast(d16, MulFixedPoint15(BitCast(s16, b_l), BitCast(s16, coeff2))));
    //    yl       = ShiftRight<1>(Add(Add(yl, g_l), half));
    yl      = AverageRound(yl, g_l);
    auto yh = BitCast(d16, MulFixedPoint15(BitCast(s16, r_h), BitCast(s16, coeff0)));
    yh      = Add(yh, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), BitCast(s16, coeff1))));
    yh      = Add(yh, BitCast(d16, MulFixedPoint15(BitCast(s16, b_h), BitCast(s16, coeff2))));
    yh      = AverageRound(yh, g_h);

    auto cbl = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, r_l), BitCast(s16, coeff3))));
    cbl      = Sub(cbl, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), BitCast(s16, coeff4))));
    cbl      = AverageRound(b_l, cbl);
    auto cbh = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, r_h), BitCast(s16, coeff3))));
    cbh      = Sub(cbh, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), BitCast(s16, coeff4))));
    cbh      = AverageRound(b_h, cbh);

    auto crl = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, g_l), BitCast(s16, coeff6))));
    crl      = Sub(crl, BitCast(d16, MulFixedPoint15(BitCast(s16, b_l), BitCast(s16, coeff7))));
    crl      = AverageRound(r_l, crl);
    auto crh = Sub(scaled_128, BitCast(d16, MulFixedPoint15(BitCast(s16, g_h), BitCast(s16, coeff6))));
    crh      = Sub(crh, BitCast(d16, MulFixedPoint15(BitCast(s16, b_h), BitCast(s16, coeff7))));
    crh      = AverageRound(r_h, crh);

    v0 = OrderedTruncate2To(d8, yl, yh);
    v1 = OrderedDemote2To(d8, cbl, cbh);
    v2 = OrderedDemote2To(d8, crl, crh);
    StoreInterleaved3(v0, v1, v2, d8, in);

    in += 3 * N;
  }
}

HWY_ATTR void subsample_simd(uint8_t *HWY_RESTRICT in, std::vector<int16_t *> out, int width, int YCCtype) {
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
  const hn::FixedTag<uint8_t, 16> u8;
  const hn::FixedTag<int16_t, 8> s16;
  auto c128  = Set(u8, 128);
  auto vhalf = Set(s16, half);
  switch (YCCtype) {
    case YCC::GRAY:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 0);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 1);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 2);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 3);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 4);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 5);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 6);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 7);
          pos += 64;
        }
      }
      break;
    case YCC::YUV444:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[1] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_1)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_2)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[2] + pos + 8 * 15);
          pos += 128;
        }
      }
      break;
    case YCC::YUV422:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 2) {
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 15);

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

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
          cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          cb71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<1>(Add(Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01)), vhalf)), s16, out[2] + pos_Chroma + 8 * 0);
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
        for (int j = 0; j < width; j += DCTSIZE * 2) {
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 15);

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

          Store(hn::ShiftRight<1>(Add(Add(cb00, cb10), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(Add(cb20, cb30), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(Add(cb40, cb50), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(Add(cb60, cb70), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(Add(cb01, cb11), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 8);
          Store(hn::ShiftRight<1>(Add(Add(cb21, cb31), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 9);
          Store(hn::ShiftRight<1>(Add(Add(cb41, cb51), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 10);
          Store(hn::ShiftRight<1>(Add(Add(cb61, cb71), Set(s16, 1))), s16, out[1] + pos_Chroma + 8 * 11);

          cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          cb20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          cb30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          cb40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          cb50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          cb60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          cb70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          cb71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

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
        for (int j = 0; j < width; j += DCTSIZE * 2) {
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
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 8);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 9);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v2_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 10);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v3_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 11);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v4_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 12);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v5_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 13);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v6_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 14);
          Store(Sub(PromoteTo(s16, UpperHalf(u8, v7_0)), PromoteTo(s16, UpperHalf(u8, c128))), s16,
                out[0] + pos + 8 * 15);
          // clang-format on

          auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb20 = Sub(PromoteTo(s16, LowerHalf(v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb30 = Sub(PromoteTo(s16, LowerHalf(v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb40 = Sub(PromoteTo(s16, LowerHalf(v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb50 = Sub(PromoteTo(s16, LowerHalf(v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb60 = Sub(PromoteTo(s16, LowerHalf(v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb70 = Sub(PromoteTo(s16, LowerHalf(v7_1)), PromoteTo(s16, LowerHalf(c128)));
          auto cb71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_1)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb00, cb10)), BitCast(s16, Add(cb01, cb11))), vhalf)), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb20, cb30)), BitCast(s16, Add(cb21, cb31))), vhalf)), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb40, cb50)), BitCast(s16, Add(cb41, cb51))), vhalf)), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cb60, cb70)), BitCast(s16, Add(cb61, cb71))), vhalf)), s16, out[1] + pos_Chroma + 8 * 3);
          // clang-format on

          auto cr00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr20 = Sub(PromoteTo(s16, LowerHalf(v2_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr21 = Sub(PromoteTo(s16, UpperHalf(u8, v2_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr30 = Sub(PromoteTo(s16, LowerHalf(v3_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr31 = Sub(PromoteTo(s16, UpperHalf(u8, v3_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr40 = Sub(PromoteTo(s16, LowerHalf(v4_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr41 = Sub(PromoteTo(s16, UpperHalf(u8, v4_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr50 = Sub(PromoteTo(s16, LowerHalf(v5_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr51 = Sub(PromoteTo(s16, UpperHalf(u8, v5_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr60 = Sub(PromoteTo(s16, LowerHalf(v6_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr61 = Sub(PromoteTo(s16, UpperHalf(u8, v6_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr70 = Sub(PromoteTo(s16, LowerHalf(v7_2)), PromoteTo(s16, LowerHalf(c128)));
          auto cr71 = Sub(PromoteTo(s16, UpperHalf(u8, v7_2)), PromoteTo(s16, LowerHalf(c128)));

          // clang-format off
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cr00, cr10)), BitCast(s16, Add(cr01, cr11))), vhalf)), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cr20, cr30)), BitCast(s16, Add(cr21, cr31))), vhalf)), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cr40, cr50)), BitCast(s16, Add(cr41, cr51))), vhalf)), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Add(Padd(s16, BitCast(s16, Add(cr60, cr70)), BitCast(s16, Add(cr61, cr71))), vhalf)), s16, out[2] + pos_Chroma + 8 * 3);
          // clang-format on

          pos += 128;
        }
      }
      break;
    case YCC::YUV411:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE * 4) {
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
            LoadInterleaved3(u8, sp + y * width * nc + nc * DCTSIZE * 2, v1_0, v1_1, v1_2);

            // Y
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p);
            Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 64);
            Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 128);
            Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 192);

            // Cb
            auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, LowerHalf(c128)));

            auto t0   = Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01));
            auto t1   = Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11));
            auto tb00 = Padd(s16, t0, t1);
            Store(hn::ShiftRight<2>(Add(Padd(s16, t0, t1), vhalf)), s16, out[1] + pos_Chroma + p);

            // Cr
            cb00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
            cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
            cb10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
            cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, LowerHalf(c128)));

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
        for (int j = 0; j < width; j += DCTSIZE * 4) {
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
            LoadInterleaved3(u8, sp + y * width * nc + nc * DCTSIZE * 2, v1_0, v1_1, v1_2);

            // Y
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p);
            Store(Sub(PromoteTo(s16, UpperHalf(u8, v0_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 64);
            Store(Sub(PromoteTo(s16, LowerHalf(v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 128);
            Store(Sub(PromoteTo(s16, UpperHalf(u8, v1_0)), PromoteTo(s16, LowerHalf(c128))), s16,
                  out[0] + pos + p + 192);
            // Cb
            auto cb00 = Sub(PromoteTo(s16, LowerHalf(v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb10 = Sub(PromoteTo(s16, LowerHalf(v1_1)), PromoteTo(s16, LowerHalf(c128)));
            auto cb11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_1)), PromoteTo(s16, LowerHalf(c128)));

            auto tb0 = Padd(s16, BitCast(s16, cb00), BitCast(s16, cb01));
            auto tb1 = Padd(s16, BitCast(s16, cb10), BitCast(s16, cb11));

            // Cr
            auto cr00 = Sub(PromoteTo(s16, LowerHalf(v0_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr01 = Sub(PromoteTo(s16, UpperHalf(u8, v0_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr10 = Sub(PromoteTo(s16, LowerHalf(v1_2)), PromoteTo(s16, LowerHalf(c128)));
            auto cr11 = Sub(PromoteTo(s16, UpperHalf(u8, v1_2)), PromoteTo(s16, LowerHalf(c128)));

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
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in + nc * i * width + nc * j;
          for (int y = 0; y < DCTSIZE; ++y) {
            // clang-format off
            auto v0_0 = Undefined(u8); auto v0_1 = Undefined(u8); auto v0_2 = Undefined(u8);
            // clang-format on

            LoadInterleaved3(u8, sp + y * width * nc, v0_0, v0_1, v0_2);
            Store(Sub(PromoteTo(s16, LowerHalf(v0_0)), PromoteTo(s16, LowerHalf(c128))), s16, out[0] + pos);
            pos += 8;
          }
        }
      }
      break;
    default:  // Shall not reach here
      break;
  }
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(rgb2ycbcr_simd);
HWY_EXPORT(subsample_simd);

void rgb2ycbcr(uint8_t *HWY_RESTRICT in, int width) { HWY_DYNAMIC_DISPATCH(rgb2ycbcr_simd)(in, width); }
void subsample(uint8_t *HWY_RESTRICT in, std::vector<int16_t *> out, int width, int YCCtype) {
  HWY_DYNAMIC_DISPATCH(subsample_simd)(in, std::move(out), width, YCCtype);
};
}  // namespace jpegenc_hwy
#endif