// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "color.cpp"  // this file
#include <hwy/foreach_target.h>         // must come before highway.h

#include <hwy/highway.h>

#include "color.hpp"
#include "ycctype.hpp"
#include "constants.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

#define MulFixedPoint15U(x, i) BitCast(u16, MulFixedPoint15(BitCast(s16, (x)), hn::Broadcast<i>(coeffs)))

#if HWY_TARGET != HWY_SCALAR
HWY_ATTR void rgb2ycbcr(uint8_t *HWY_RESTRICT in, std::vector<uint8_t *> &out, int width) {
  const hn::ScalableTag<uint8_t> u8;
  const hn::ScalableTag<uint16_t> u16;
  const hn::ScalableTag<int16_t> s16;

  HWY_ALIGN constexpr int16_t constants[] = {19595, 38470 - 32768, 7471, 11059, 21709, 0, 27439, 5329};
  const auto coeffs                       = LoadDup128(s16, constants);
  // clang-format off
  //  const auto coeffs = hn::Dup128VecFromValues(s16, 19595, 38470 - 32768, 7471, 11059, 21709, 0, 27439, 5329);
  // clang-format on
  const auto scaled_128_1 = Set(u16, (128 << 1) + 0);

  auto v0 = Undefined(u8);
  auto v1 = Undefined(u8);
  auto v2 = Undefined(u8);

  uint8_t *HWY_RESTRICT o0 = out[0];
  uint8_t *HWY_RESTRICT o1 = out[1];
  uint8_t *HWY_RESTRICT o2 = out[2];

  constexpr size_t N       = Lanes(u8);
  const size_t num_samples = width * LINES;

  for (size_t i = num_samples; i > 0; i -= N) {
    LoadInterleaved3(u8, in, v0, v1, v2);

    // clang-format off
    auto r_l  = PromoteLowerTo(u16, v0); auto g_l  = PromoteLowerTo(u16, v1); auto b_l  = PromoteLowerTo(u16, v2);
    auto r_h  = PromoteUpperTo(u16, v0); auto g_h  = PromoteUpperTo(u16, v1); auto b_h  = PromoteUpperTo(u16, v2);
    // clang-format on

    auto yl = MulFixedPoint15U(r_l, 0);
    yl      = Add(yl, MulFixedPoint15U(g_l, 1));
    yl      = Add(yl, MulFixedPoint15U(b_l, 2));
    yl      = hn::ShiftRight<1>(Add(yl, g_l));
    auto yh = BitCast(u16, MulFixedPoint15U(r_h, 0));
    yh      = Add(yh, MulFixedPoint15U(g_h, 1));
    yh      = Add(yh, MulFixedPoint15U(b_h, 2));
    yh      = hn::ShiftRight<1>(Add(yh, g_h));
    v0      = OrderedTruncate2To(u8, yl, yh);
    Store(v0, u8, o0);

    auto cbl = Sub(scaled_128_1, MulFixedPoint15U(r_l, 3));
    cbl      = Sub(cbl, MulFixedPoint15U(g_l, 4));
    cbl      = AverageRound(b_l, cbl);
    auto cbh = Sub(scaled_128_1, MulFixedPoint15U(r_h, 3));
    cbh      = Sub(cbh, MulFixedPoint15U(g_h, 4));
    cbh      = AverageRound(b_h, cbh);
    v1       = OrderedTruncate2To(u8, cbl, cbh);
    Store(v1, u8, o1);

    auto crl = Sub(scaled_128_1, MulFixedPoint15U(g_l, 6));
    crl      = Sub(crl, MulFixedPoint15U(b_l, 7));
    crl      = AverageRound(r_l, crl);
    auto crh = Sub(scaled_128_1, MulFixedPoint15U(g_h, 6));
    crh      = Sub(crh, MulFixedPoint15U(b_h, 7));
    crh      = AverageRound(r_h, crh);
    v2       = OrderedTruncate2To(u8, crl, crh);
    Store(v2, u8, o2);

    //    StoreInterleaved3(v0, v1, v2, u8, in);

    in += 3 * N;
    o0 += N;
    o1 += N;
    o2 += N;
  }
}

/*
 Subsampling operation arranges component sample values in MCU order.
 (In other words, component samples in an MCU are 1-d contiguous array.)
 */
HWY_ATTR void subsample_core(std::vector<uint8_t *> &in, std::vector<int16_t *> &out, int width,
                             int YCCtype) {
  size_t pos        = 0;
  size_t pos_Chroma = 0;

  hn::FixedTag<uint8_t, 16> u8;
  hn::FixedTag<int16_t, 8> s16;
  const auto c128 = Set(s16, 128);

  switch (YCCtype) {
    case YCC::GRAY:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp = in[0] + i * width + j;
          auto v0 = Load(u8, sp + 0 * width);
          auto v1 = Load(u8, sp + 1 * width);
          auto v2 = Load(u8, sp + 2 * width);
          auto v3 = Load(u8, sp + 3 * width);
          auto v4 = Load(u8, sp + 4 * width);
          auto v5 = Load(u8, sp + 5 * width);
          auto v6 = Load(u8, sp + 6 * width);
          auto v7 = Load(u8, sp + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7), c128), s16, out[0] + pos + 8 * 7);
          pos += 64;
          Store(Sub(PromoteUpperTo(s16, v0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteUpperTo(s16, v1), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteUpperTo(s16, v2), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteUpperTo(s16, v3), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteUpperTo(s16, v4), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteUpperTo(s16, v5), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteUpperTo(s16, v6), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteUpperTo(s16, v7), c128), s16, out[0] + pos + 8 * 7);
          pos += 64;
        }
      }
      break;
    case YCC::YUV444:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp0 = in[0] + i * width + j;
          auto sp1 = in[1] + i * width + j;
          auto sp2 = in[2] + i * width + j;

          auto v0_0 = Load(u8, sp0 + 0 * width);
          auto v1_0 = Load(u8, sp0 + 1 * width);
          auto v2_0 = Load(u8, sp0 + 2 * width);
          auto v3_0 = Load(u8, sp0 + 3 * width);
          auto v4_0 = Load(u8, sp0 + 4 * width);
          auto v5_0 = Load(u8, sp0 + 5 * width);
          auto v6_0 = Load(u8, sp0 + 6 * width);
          auto v7_0 = Load(u8, sp0 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 8);
          Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 9);
          Store(Sub(PromoteUpperTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 10);
          Store(Sub(PromoteUpperTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 11);
          Store(Sub(PromoteUpperTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 12);
          Store(Sub(PromoteUpperTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 13);
          Store(Sub(PromoteUpperTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 14);
          Store(Sub(PromoteUpperTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 15);

          auto v0_1 = Load(u8, sp1 + 0 * width);
          auto v1_1 = Load(u8, sp1 + 1 * width);
          auto v2_1 = Load(u8, sp1 + 2 * width);
          auto v3_1 = Load(u8, sp1 + 3 * width);
          auto v4_1 = Load(u8, sp1 + 4 * width);
          auto v5_1 = Load(u8, sp1 + 5 * width);
          auto v6_1 = Load(u8, sp1 + 6 * width);
          auto v7_1 = Load(u8, sp1 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_1), c128), s16, out[1] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_1), c128), s16, out[1] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_1), c128), s16, out[1] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_1), c128), s16, out[1] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_1), c128), s16, out[1] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_1), c128), s16, out[1] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_1), c128), s16, out[1] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_1), c128), s16, out[1] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_1), c128), s16, out[1] + pos + 8 * 8);
          Store(Sub(PromoteUpperTo(s16, v1_1), c128), s16, out[1] + pos + 8 * 9);
          Store(Sub(PromoteUpperTo(s16, v2_1), c128), s16, out[1] + pos + 8 * 10);
          Store(Sub(PromoteUpperTo(s16, v3_1), c128), s16, out[1] + pos + 8 * 11);
          Store(Sub(PromoteUpperTo(s16, v4_1), c128), s16, out[1] + pos + 8 * 12);
          Store(Sub(PromoteUpperTo(s16, v5_1), c128), s16, out[1] + pos + 8 * 13);
          Store(Sub(PromoteUpperTo(s16, v6_1), c128), s16, out[1] + pos + 8 * 14);
          Store(Sub(PromoteUpperTo(s16, v7_1), c128), s16, out[1] + pos + 8 * 15);

          auto v0_2 = Load(u8, sp2 + 0 * width);
          auto v1_2 = Load(u8, sp2 + 1 * width);
          auto v2_2 = Load(u8, sp2 + 2 * width);
          auto v3_2 = Load(u8, sp2 + 3 * width);
          auto v4_2 = Load(u8, sp2 + 4 * width);
          auto v5_2 = Load(u8, sp2 + 5 * width);
          auto v6_2 = Load(u8, sp2 + 6 * width);
          auto v7_2 = Load(u8, sp2 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_2), c128), s16, out[2] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_2), c128), s16, out[2] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_2), c128), s16, out[2] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_2), c128), s16, out[2] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_2), c128), s16, out[2] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_2), c128), s16, out[2] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_2), c128), s16, out[2] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_2), c128), s16, out[2] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_2), c128), s16, out[2] + pos + 8 * 8);
          Store(Sub(PromoteUpperTo(s16, v1_2), c128), s16, out[2] + pos + 8 * 9);
          Store(Sub(PromoteUpperTo(s16, v2_2), c128), s16, out[2] + pos + 8 * 10);
          Store(Sub(PromoteUpperTo(s16, v3_2), c128), s16, out[2] + pos + 8 * 11);
          Store(Sub(PromoteUpperTo(s16, v4_2), c128), s16, out[2] + pos + 8 * 12);
          Store(Sub(PromoteUpperTo(s16, v5_2), c128), s16, out[2] + pos + 8 * 13);
          Store(Sub(PromoteUpperTo(s16, v6_2), c128), s16, out[2] + pos + 8 * 14);
          Store(Sub(PromoteUpperTo(s16, v7_2), c128), s16, out[2] + pos + 8 * 15);
          pos += 128;
        }
      }
      break;
    case YCC::YUV422:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8)) {
          auto sp0 = in[0] + i * width + j;
          auto sp1 = in[1] + i * width + j;
          auto sp2 = in[2] + i * width + j;

          auto v0_0 = Load(u8, sp0 + 0 * width);
          auto v1_0 = Load(u8, sp0 + 1 * width);
          auto v2_0 = Load(u8, sp0 + 2 * width);
          auto v3_0 = Load(u8, sp0 + 3 * width);
          auto v4_0 = Load(u8, sp0 + 4 * width);
          auto v5_0 = Load(u8, sp0 + 5 * width);
          auto v6_0 = Load(u8, sp0 + 6 * width);
          auto v7_0 = Load(u8, sp0 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 8);
          Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 9);
          Store(Sub(PromoteUpperTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 10);
          Store(Sub(PromoteUpperTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 11);
          Store(Sub(PromoteUpperTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 12);
          Store(Sub(PromoteUpperTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 13);
          Store(Sub(PromoteUpperTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 14);
          Store(Sub(PromoteUpperTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 15);

          auto v0_1 = Load(u8, sp1 + 0 * width);
          auto v1_1 = Load(u8, sp1 + 1 * width);
          auto v2_1 = Load(u8, sp1 + 2 * width);
          auto v3_1 = Load(u8, sp1 + 3 * width);
          auto v4_1 = Load(u8, sp1 + 4 * width);
          auto v5_1 = Load(u8, sp1 + 5 * width);
          auto v6_1 = Load(u8, sp1 + 6 * width);
          auto v7_1 = Load(u8, sp1 + 7 * width);

          auto cb00 = Sub(PromoteLowerTo(s16, v0_1), c128);
          auto cb01 = Sub(PromoteUpperTo(s16, v0_1), c128);
          auto cb10 = Sub(PromoteLowerTo(s16, v1_1), c128);
          auto cb11 = Sub(PromoteUpperTo(s16, v1_1), c128);
          auto cb20 = Sub(PromoteLowerTo(s16, v2_1), c128);
          auto cb21 = Sub(PromoteUpperTo(s16, v2_1), c128);
          auto cb30 = Sub(PromoteLowerTo(s16, v3_1), c128);
          auto cb31 = Sub(PromoteUpperTo(s16, v3_1), c128);
          auto cb40 = Sub(PromoteLowerTo(s16, v4_1), c128);
          auto cb41 = Sub(PromoteUpperTo(s16, v4_1), c128);
          auto cb50 = Sub(PromoteLowerTo(s16, v5_1), c128);
          auto cb51 = Sub(PromoteUpperTo(s16, v5_1), c128);
          auto cb60 = Sub(PromoteLowerTo(s16, v6_1), c128);
          auto cb61 = Sub(PromoteUpperTo(s16, v6_1), c128);
          auto cb70 = Sub(PromoteLowerTo(s16, v7_1), c128);
          auto cb71 = Sub(PromoteUpperTo(s16, v7_1), c128);

          // clang-format off
          Store(hn::ShiftRight<1>(Padd(s16, cb00, cb01)), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Padd(s16, cb10, cb11)), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Padd(s16, cb20, cb21)), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Padd(s16, cb30, cb31)), s16, out[1] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Padd(s16, cb40, cb41)), s16, out[1] + pos_Chroma + 8 * 4);
          Store(hn::ShiftRight<1>(Padd(s16, cb50, cb51)), s16, out[1] + pos_Chroma + 8 * 5);
          Store(hn::ShiftRight<1>(Padd(s16, cb60, cb61)), s16, out[1] + pos_Chroma + 8 * 6);
          Store(hn::ShiftRight<1>(Padd(s16, cb70, cb71)), s16, out[1] + pos_Chroma + 8 * 7);
          // clang-format on

          auto v0_2 = Load(u8, sp2 + 0 * width);
          auto v1_2 = Load(u8, sp2 + 1 * width);
          auto v2_2 = Load(u8, sp2 + 2 * width);
          auto v3_2 = Load(u8, sp2 + 3 * width);
          auto v4_2 = Load(u8, sp2 + 4 * width);
          auto v5_2 = Load(u8, sp2 + 5 * width);
          auto v6_2 = Load(u8, sp2 + 6 * width);
          auto v7_2 = Load(u8, sp2 + 7 * width);

          cb00 = Sub(PromoteLowerTo(s16, v0_2), c128);
          cb01 = Sub(PromoteUpperTo(s16, v0_2), c128);
          cb10 = Sub(PromoteLowerTo(s16, v1_2), c128);
          cb11 = Sub(PromoteUpperTo(s16, v1_2), c128);
          cb20 = Sub(PromoteLowerTo(s16, v2_2), c128);
          cb21 = Sub(PromoteUpperTo(s16, v2_2), c128);
          cb30 = Sub(PromoteLowerTo(s16, v3_2), c128);
          cb31 = Sub(PromoteUpperTo(s16, v3_2), c128);
          cb40 = Sub(PromoteLowerTo(s16, v4_2), c128);
          cb41 = Sub(PromoteUpperTo(s16, v4_2), c128);
          cb50 = Sub(PromoteLowerTo(s16, v5_2), c128);
          cb51 = Sub(PromoteUpperTo(s16, v5_2), c128);
          cb60 = Sub(PromoteLowerTo(s16, v6_2), c128);
          cb61 = Sub(PromoteUpperTo(s16, v6_2), c128);
          cb70 = Sub(PromoteLowerTo(s16, v7_2), c128);
          cb71 = Sub(PromoteUpperTo(s16, v7_2), c128);

          // clang-format off
          Store(hn::ShiftRight<1>(Padd(s16, cb00, cb01)), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Padd(s16, cb10, cb11)), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Padd(s16, cb20, cb21)), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Padd(s16, cb30, cb31)), s16, out[2] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Padd(s16, cb40, cb41)), s16, out[2] + pos_Chroma + 8 * 4);
          Store(hn::ShiftRight<1>(Padd(s16, cb50, cb51)), s16, out[2] + pos_Chroma + 8 * 5);
          Store(hn::ShiftRight<1>(Padd(s16, cb60, cb61)), s16, out[2] + pos_Chroma + 8 * 6);
          Store(hn::ShiftRight<1>(Padd(s16, cb70, cb71)), s16, out[2] + pos_Chroma + 8 * 7);
          // clang-format on
          pos += 128;
          pos_Chroma += 64;
        }
      }
      break;
    case YCC::YUV440:
      for (int j = 0; j < width; j += Lanes(u8)) {
        for (int i = 0; i < LINES; i += DCTSIZE) {
          auto sp0   = in[0] + i * width + j;
          auto sp1   = in[1] + i * width + j;
          auto sp2   = in[2] + i * width + j;
          pos        = j * 16 + i * 8;
          pos_Chroma = j * 8 + i * 4;

          auto v0_0 = Load(u8, sp0 + 0 * width);
          auto v1_0 = Load(u8, sp0 + 1 * width);
          auto v2_0 = Load(u8, sp0 + 2 * width);
          auto v3_0 = Load(u8, sp0 + 3 * width);
          auto v4_0 = Load(u8, sp0 + 4 * width);
          auto v5_0 = Load(u8, sp0 + 5 * width);
          auto v6_0 = Load(u8, sp0 + 6 * width);
          auto v7_0 = Load(u8, sp0 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 0 + 128);
          Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 1 + 128);
          Store(Sub(PromoteUpperTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 2 + 128);
          Store(Sub(PromoteUpperTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 3 + 128);
          Store(Sub(PromoteUpperTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 4 + 128);
          Store(Sub(PromoteUpperTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 5 + 128);
          Store(Sub(PromoteUpperTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 6 + 128);
          Store(Sub(PromoteUpperTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 7 + 128);

          auto v0_1 = Load(u8, sp1 + 0 * width);
          auto v1_1 = Load(u8, sp1 + 1 * width);
          auto v2_1 = Load(u8, sp1 + 2 * width);
          auto v3_1 = Load(u8, sp1 + 3 * width);
          auto v4_1 = Load(u8, sp1 + 4 * width);
          auto v5_1 = Load(u8, sp1 + 5 * width);
          auto v6_1 = Load(u8, sp1 + 6 * width);
          auto v7_1 = Load(u8, sp1 + 7 * width);

          auto cb00 = Sub(PromoteLowerTo(s16, v0_1), c128);
          auto cb01 = Sub(PromoteUpperTo(s16, v0_1), c128);
          auto cb10 = Sub(PromoteLowerTo(s16, v1_1), c128);
          auto cb11 = Sub(PromoteUpperTo(s16, v1_1), c128);
          auto cb20 = Sub(PromoteLowerTo(s16, v2_1), c128);
          auto cb21 = Sub(PromoteUpperTo(s16, v2_1), c128);
          auto cb30 = Sub(PromoteLowerTo(s16, v3_1), c128);
          auto cb31 = Sub(PromoteUpperTo(s16, v3_1), c128);
          auto cb40 = Sub(PromoteLowerTo(s16, v4_1), c128);
          auto cb41 = Sub(PromoteUpperTo(s16, v4_1), c128);
          auto cb50 = Sub(PromoteLowerTo(s16, v5_1), c128);
          auto cb51 = Sub(PromoteUpperTo(s16, v5_1), c128);
          auto cb60 = Sub(PromoteLowerTo(s16, v6_1), c128);
          auto cb61 = Sub(PromoteUpperTo(s16, v6_1), c128);
          auto cb70 = Sub(PromoteLowerTo(s16, v7_1), c128);
          auto cb71 = Sub(PromoteUpperTo(s16, v7_1), c128);

          Store(hn::ShiftRight<1>(Add(cb00, cb10)), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(cb20, cb30)), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(cb40, cb50)), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(cb60, cb70)), s16, out[1] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(cb01, cb11)), s16, out[1] + pos_Chroma + 8 * 8);
          Store(hn::ShiftRight<1>(Add(cb21, cb31)), s16, out[1] + pos_Chroma + 8 * 9);
          Store(hn::ShiftRight<1>(Add(cb41, cb51)), s16, out[1] + pos_Chroma + 8 * 10);
          Store(hn::ShiftRight<1>(Add(cb61, cb71)), s16, out[1] + pos_Chroma + 8 * 11);

          auto v0_2 = Load(u8, sp2 + 0 * width);
          auto v1_2 = Load(u8, sp2 + 1 * width);
          auto v2_2 = Load(u8, sp2 + 2 * width);
          auto v3_2 = Load(u8, sp2 + 3 * width);
          auto v4_2 = Load(u8, sp2 + 4 * width);
          auto v5_2 = Load(u8, sp2 + 5 * width);
          auto v6_2 = Load(u8, sp2 + 6 * width);
          auto v7_2 = Load(u8, sp2 + 7 * width);

          cb00 = Sub(PromoteLowerTo(s16, v0_2), c128);
          cb01 = Sub(PromoteUpperTo(s16, v0_2), c128);
          cb10 = Sub(PromoteLowerTo(s16, v1_2), c128);
          cb11 = Sub(PromoteUpperTo(s16, v1_2), c128);
          cb20 = Sub(PromoteLowerTo(s16, v2_2), c128);
          cb21 = Sub(PromoteUpperTo(s16, v2_2), c128);
          cb30 = Sub(PromoteLowerTo(s16, v3_2), c128);
          cb31 = Sub(PromoteUpperTo(s16, v3_2), c128);
          cb40 = Sub(PromoteLowerTo(s16, v4_2), c128);
          cb41 = Sub(PromoteUpperTo(s16, v4_2), c128);
          cb50 = Sub(PromoteLowerTo(s16, v5_2), c128);
          cb51 = Sub(PromoteUpperTo(s16, v5_2), c128);
          cb60 = Sub(PromoteLowerTo(s16, v6_2), c128);
          cb61 = Sub(PromoteUpperTo(s16, v6_2), c128);
          cb70 = Sub(PromoteLowerTo(s16, v7_2), c128);
          cb71 = Sub(PromoteUpperTo(s16, v7_2), c128);

          Store(hn::ShiftRight<1>(Add(cb00, cb10)), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<1>(Add(cb20, cb30)), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<1>(Add(cb40, cb50)), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<1>(Add(cb60, cb70)), s16, out[2] + pos_Chroma + 8 * 3);
          Store(hn::ShiftRight<1>(Add(cb01, cb11)), s16, out[2] + pos_Chroma + 8 * 8);
          Store(hn::ShiftRight<1>(Add(cb21, cb31)), s16, out[2] + pos_Chroma + 8 * 9);
          Store(hn::ShiftRight<1>(Add(cb41, cb51)), s16, out[2] + pos_Chroma + 8 * 10);
          Store(hn::ShiftRight<1>(Add(cb61, cb71)), s16, out[2] + pos_Chroma + 8 * 11);
        }
      }
      break;
    case YCC::YUV420:
      for (int j = 0; j < width; j += Lanes(u8)) {
        for (int i = 0; i < LINES; i += DCTSIZE) {
          auto sp0   = in[0] + i * width + j;
          auto sp1   = in[1] + i * width + j;
          auto sp2   = in[2] + i * width + j;
          pos_Chroma = j * 4 + i * 4;

          auto v0_0 = Load(u8, sp0 + 0 * width);
          auto v1_0 = Load(u8, sp0 + 1 * width);
          auto v2_0 = Load(u8, sp0 + 2 * width);
          auto v3_0 = Load(u8, sp0 + 3 * width);
          auto v4_0 = Load(u8, sp0 + 4 * width);
          auto v5_0 = Load(u8, sp0 + 5 * width);
          auto v6_0 = Load(u8, sp0 + 6 * width);
          auto v7_0 = Load(u8, sp0 + 7 * width);

          Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 0);
          Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 1);
          Store(Sub(PromoteLowerTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 2);
          Store(Sub(PromoteLowerTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 3);
          Store(Sub(PromoteLowerTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 4);
          Store(Sub(PromoteLowerTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 5);
          Store(Sub(PromoteLowerTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 6);
          Store(Sub(PromoteLowerTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 7);
          Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + 8 * 8);
          Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + 8 * 9);
          Store(Sub(PromoteUpperTo(s16, v2_0), c128), s16, out[0] + pos + 8 * 10);
          Store(Sub(PromoteUpperTo(s16, v3_0), c128), s16, out[0] + pos + 8 * 11);
          Store(Sub(PromoteUpperTo(s16, v4_0), c128), s16, out[0] + pos + 8 * 12);
          Store(Sub(PromoteUpperTo(s16, v5_0), c128), s16, out[0] + pos + 8 * 13);
          Store(Sub(PromoteUpperTo(s16, v6_0), c128), s16, out[0] + pos + 8 * 14);
          Store(Sub(PromoteUpperTo(s16, v7_0), c128), s16, out[0] + pos + 8 * 15);

          auto v0_1 = Load(u8, sp1 + 0 * width);
          auto v1_1 = Load(u8, sp1 + 1 * width);
          auto v2_1 = Load(u8, sp1 + 2 * width);
          auto v3_1 = Load(u8, sp1 + 3 * width);
          auto v4_1 = Load(u8, sp1 + 4 * width);
          auto v5_1 = Load(u8, sp1 + 5 * width);
          auto v6_1 = Load(u8, sp1 + 6 * width);
          auto v7_1 = Load(u8, sp1 + 7 * width);

          auto cb00 = Sub(PromoteLowerTo(s16, v0_1), c128);
          auto cb01 = Sub(PromoteUpperTo(s16, v0_1), c128);
          auto cb10 = Sub(PromoteLowerTo(s16, v1_1), c128);
          auto cb11 = Sub(PromoteUpperTo(s16, v1_1), c128);
          auto cb20 = Sub(PromoteLowerTo(s16, v2_1), c128);
          auto cb21 = Sub(PromoteUpperTo(s16, v2_1), c128);
          auto cb30 = Sub(PromoteLowerTo(s16, v3_1), c128);
          auto cb31 = Sub(PromoteUpperTo(s16, v3_1), c128);
          auto cb40 = Sub(PromoteLowerTo(s16, v4_1), c128);
          auto cb41 = Sub(PromoteUpperTo(s16, v4_1), c128);
          auto cb50 = Sub(PromoteLowerTo(s16, v5_1), c128);
          auto cb51 = Sub(PromoteUpperTo(s16, v5_1), c128);
          auto cb60 = Sub(PromoteLowerTo(s16, v6_1), c128);
          auto cb61 = Sub(PromoteUpperTo(s16, v6_1), c128);
          auto cb70 = Sub(PromoteLowerTo(s16, v7_1), c128);
          auto cb71 = Sub(PromoteUpperTo(s16, v7_1), c128);

          // clang-format off
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb00, cb10), Add(cb01, cb11))), s16, out[1] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb20, cb30), Add(cb21, cb31))), s16, out[1] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb40, cb50), Add(cb41, cb51))), s16, out[1] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb60, cb70), Add(cb61, cb71))), s16, out[1] + pos_Chroma + 8 * 3);
          // clang-format on

          auto v0_2 = Load(u8, sp2 + 0 * width);
          auto v1_2 = Load(u8, sp2 + 1 * width);
          auto v2_2 = Load(u8, sp2 + 2 * width);
          auto v3_2 = Load(u8, sp2 + 3 * width);
          auto v4_2 = Load(u8, sp2 + 4 * width);
          auto v5_2 = Load(u8, sp2 + 5 * width);
          auto v6_2 = Load(u8, sp2 + 6 * width);
          auto v7_2 = Load(u8, sp2 + 7 * width);

          cb00 = Sub(PromoteLowerTo(s16, v0_2), c128);
          cb01 = Sub(PromoteUpperTo(s16, v0_2), c128);
          cb10 = Sub(PromoteLowerTo(s16, v1_2), c128);
          cb11 = Sub(PromoteUpperTo(s16, v1_2), c128);
          cb20 = Sub(PromoteLowerTo(s16, v2_2), c128);
          cb21 = Sub(PromoteUpperTo(s16, v2_2), c128);
          cb30 = Sub(PromoteLowerTo(s16, v3_2), c128);
          cb31 = Sub(PromoteUpperTo(s16, v3_2), c128);
          cb40 = Sub(PromoteLowerTo(s16, v4_2), c128);
          cb41 = Sub(PromoteUpperTo(s16, v4_2), c128);
          cb50 = Sub(PromoteLowerTo(s16, v5_2), c128);
          cb51 = Sub(PromoteUpperTo(s16, v5_2), c128);
          cb60 = Sub(PromoteLowerTo(s16, v6_2), c128);
          cb61 = Sub(PromoteUpperTo(s16, v6_2), c128);
          cb70 = Sub(PromoteLowerTo(s16, v7_2), c128);
          cb71 = Sub(PromoteUpperTo(s16, v7_2), c128);

          // clang-format off
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb00, cb10), Add(cb01, cb11))), s16, out[2] + pos_Chroma + 8 * 0);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb20, cb30), Add(cb21, cb31))), s16, out[2] + pos_Chroma + 8 * 1);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb40, cb50), Add(cb41, cb51))), s16, out[2] + pos_Chroma + 8 * 2);
          Store(hn::ShiftRight<2>(Padd(s16, Add(cb60, cb70), Add(cb61, cb71))), s16, out[2] + pos_Chroma + 8 * 3);
          // clang-format on

          pos += 128;
        }
      }
      break;
    case YCC::YUV411:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += Lanes(u8) * 2) {
          auto sp0 = in[0] + i * width + j;
          auto sp1 = in[1] + i * width + j;
          auto sp2 = in[2] + i * width + j;

          size_t p = 0;
          for (int y = 0; y < DCTSIZE; ++y) {
            auto v0_0 = Load(u8, sp0 + y * width);
            auto v0_1 = Load(u8, sp1 + y * width);
            auto v0_2 = Load(u8, sp2 + y * width);
            auto v1_0 = Load(u8, sp0 + y * width + Lanes(u8));
            auto v1_1 = Load(u8, sp1 + y * width + Lanes(u8));
            auto v1_2 = Load(u8, sp2 + y * width + Lanes(u8));

            // Y
            Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + p);
            Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + p + 64);
            Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + p + 128);
            Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + p + 192);

            // Cb
            auto cb00 = Sub(PromoteLowerTo(s16, v0_1), c128);
            auto cb01 = Sub(PromoteUpperTo(s16, v0_1), c128);
            auto cb10 = Sub(PromoteLowerTo(s16, v1_1), c128);
            auto cb11 = Sub(PromoteUpperTo(s16, v1_1), c128);

            auto t0   = Padd(s16, cb00, cb01);
            auto t1   = Padd(s16, cb10, cb11);
            auto tb00 = Padd(s16, t0, t1);
            Store(hn::ShiftRight<2>(Padd(s16, t0, t1)), s16, out[1] + pos_Chroma + p);

            // Cr
            cb00 = Sub(PromoteLowerTo(s16, v0_2), c128);
            cb01 = Sub(PromoteUpperTo(s16, v0_2), c128);
            cb10 = Sub(PromoteLowerTo(s16, v1_2), c128);
            cb11 = Sub(PromoteUpperTo(s16, v1_2), c128);

            t0   = Padd(s16, cb00, cb01);
            t1   = Padd(s16, cb10, cb11);
            tb00 = Padd(s16, t0, t1);
            Store(hn::ShiftRight<2>(Padd(s16, t0, t1)), s16, out[2] + pos_Chroma + p);
            p += 8;
          }
          pos += 256;
          pos_Chroma += 64;
        }
      }
      break;
    case YCC::YUV410:
      for (int j = 0; j < width; j += Lanes(u8) * 2) {
        for (int i = 0; i < LINES; i += DCTSIZE) {
          auto sp0 = in[0] + i * width + j;
          auto sp1 = in[1] + i * width + j;
          auto sp2 = in[2] + i * width + j;
          size_t p = 0, pc = 0;
          pos_Chroma = j * 2 + i * 4;
          auto cb    = Undefined(s16);
          auto cr    = Undefined(s16);
          for (int y = 0; y < DCTSIZE; ++y) {
            auto v0_0 = Load(u8, sp0 + y * width);
            auto v0_1 = Load(u8, sp1 + y * width);
            auto v0_2 = Load(u8, sp2 + y * width);
            auto v1_0 = Load(u8, sp0 + y * width + Lanes(u8));
            auto v1_1 = Load(u8, sp1 + y * width + Lanes(u8));
            auto v1_2 = Load(u8, sp2 + y * width + Lanes(u8));

            // Y
            Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + p);
            Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + p + 64);
            Store(Sub(PromoteLowerTo(s16, v1_0), c128), s16, out[0] + pos + p + 128);
            Store(Sub(PromoteUpperTo(s16, v1_0), c128), s16, out[0] + pos + p + 192);
            // Cb
            auto cb00 = Sub(PromoteLowerTo(s16, v0_1), c128);
            auto cb01 = Sub(PromoteUpperTo(s16, v0_1), c128);
            auto cb10 = Sub(PromoteLowerTo(s16, v1_1), c128);
            auto cb11 = Sub(PromoteUpperTo(s16, v1_1), c128);

            auto tb0 = Padd(s16, cb00, cb01);
            auto tb1 = Padd(s16, cb10, cb11);

            // Cr
            auto cr00 = Sub(PromoteLowerTo(s16, v0_2), c128);
            auto cr01 = Sub(PromoteUpperTo(s16, v0_2), c128);
            auto cr10 = Sub(PromoteLowerTo(s16, v1_2), c128);
            auto cr11 = Sub(PromoteUpperTo(s16, v1_2), c128);

            auto tr0 = Padd(s16, cr00, cr01);
            auto tr1 = Padd(s16, cr10, cr11);

            if (y % 2 == 0) {
              cb = Padd(s16, tb0, tb1);
              cr = Padd(s16, tr0, tr1);
            } else {
              cb = hn::ShiftRight<3>(Add(cb, Padd(s16, tb0, tb1)));
              cr = hn::ShiftRight<3>(Add(cr, Padd(s16, tr0, tr1)));
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
          auto sp0 = in[0] + i * width + j;
          for (int y = 0; y < DCTSIZE; ++y) {
            auto v0_0 = Load(u8, sp0 + y * width);
            Store(Sub(PromoteLowerTo(s16, v0_0), c128), s16, out[0] + pos + y * DCTSIZE);
            Store(Sub(PromoteUpperTo(s16, v0_0), c128), s16, out[0] + pos + 64 + y * DCTSIZE);
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
HWY_ATTR void rgb2ycbcr(uint8_t *HWY_RESTRICT in, std::vector<uint8_t *> &out, const int width) {
  uint8_t *I0 = in, *I1 = in + 1, *I2 = in + 2;
  uint8_t *o0 = out[0], *o1 = out[1], *o2 = out[2];
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
    o0[0] = static_cast<uint8_t>(Y);
    o1[0] = static_cast<uint8_t>(Cb + 128);
    o2[0] = static_cast<uint8_t>(Cr + 128);
    I0 += 3;
    I1 += 3;
    I2 += 3;
    o0++;
    o1++;
    o2++;
  }
}

HWY_ATTR void subsample_core(std::vector<uint8_t *> &in, std::vector<int16_t *> &out, const int width,
                             const int YCCtype) {
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
  switch (YCCtype) {
    case YCC::GRAY:
    case YCC::GRAY2:
    case YCC::YUV444:
    case YCC::YUV422:
    case YCC::YUV411:
      for (int i = 0; i < LINES; i += DCTSIZE) {
        for (int j = 0; j < width; j += DCTSIZE) {
          auto sp = in[0] + i * width + j;
          for (int y = 0; y < DCTSIZE; ++y) {
            for (int x = 0; x < DCTSIZE; ++x) {
              out[0][pos] = static_cast<int16_t>(sp[y * width + x] - 128);
              pos++;
            }
          }
        }
      }
      break;
    case YCC::YUV440:
    case YCC::YUV420:
    case YCC::YUV410:
      for (int i = 0; i < LINES; i += DCTSIZE * scale_y) {
        for (int j = 0; j < width; j += DCTSIZE * scale_x) {
          auto sp = in[0] + i * width + j;
          for (int y = 0; y < DCTSIZE * scale_y; y += DCTSIZE) {
            for (int x = 0; x < DCTSIZE * scale_x; x += DCTSIZE) {
              for (int p = 0; p < DCTSIZE; ++p) {
                for (int q = 0; q < DCTSIZE; ++q) {
                  out[0][pos] = static_cast<int16_t>(sp[(y + p) * width + (x + q)] - 128);
                  pos++;
                }
              }
            }
          }
        }
      }
      break;
  }
  // Chroma, Cb and Cr
  for (int c = 1; c < nc; ++c) {
    pos = 0;
    for (int i = 0; i < LINES; i += DCTSIZE * scale_y) {
      for (int j = 0; j < width; j += DCTSIZE * scale_x) {
        auto sp = in[c] + i * width + j;
        for (int y = 0; y < DCTSIZE * scale_y; y += scale_y) {
          for (int x = 0; x < DCTSIZE * scale_x; x += scale_x) {
            int ave = 0;
            for (int p = 0; p < scale_y; ++p) {
              for (int q = 0; q < scale_x; ++q) {
                ave += sp[(y + p) * width + (x + q)];
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

void rgb2ycbcr(uint8_t *HWY_RESTRICT in, std::vector<uint8_t *> &out, const int width) {
  HWY_DYNAMIC_DISPATCH(rgb2ycbcr)(in, out, width);
}
void subsample(std::vector<uint8_t *> &in, std::vector<int16_t *> &out, const int width,
               const int YCCtype) {
  HWY_DYNAMIC_DISPATCH(subsample_core)(in, out, width, YCCtype);
}
}  // namespace jpegenc_hwy
#endif