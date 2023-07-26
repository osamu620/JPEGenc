// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "quantization.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h

#include <hwy/highway.h>

#include <cmath>
#include "ycctype.hpp"
#include "constants.hpp"
#include "quantization.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void quantize_fwd(int16_t *HWY_RESTRICT in, const int *HWY_RESTRICT qtable) {
#if HWY_TARGET != HWY_SCALAR
  const hn::ScalableTag<int16_t> d16;
  const hn::ScalableTag<int32_t> d32;
  const size_t L16 = Lanes(d16);
  const size_t L32 = Lanes(d32);
  auto half        = hn::Set(d32, 1 << 15);
  for (int i = 0; i < DCTSIZE2; i += L16) {
    auto ql = Load(d32, qtable);
    auto qh = Load(d32, qtable + L32);
    auto v  = Load(d16, in);
    auto vl = PromoteTo(d32, LowerHalf(v));
    auto vh = PromoteTo(d32, UpperHalf(d16, v));

    vl = Mul(vl, ql);
    vh = Mul(vh, qh);
    vl = Add(vl, half);
    vh = Add(vh, half);
    vl = hn::ShiftRight<16>(vl);
    vh = hn::ShiftRight<16>(vh);
    Store(OrderedDemote2To(d16, vl, vh), d16, in);
    in += L16;
    qtable += L16;
  }
#else
  int shift = 16;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < DCTSIZE2; ++i) {
    in[i] = static_cast<int16_t>((in[i] * qtable[i] + half) >> shift);
  }
#endif
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE

namespace jpegenc_hwy {

HWY_EXPORT(quantize_fwd);
void quantize(std::vector<int16_t *> in, int *qtableL, int *qtableC, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;

  for (int i = 0; i < width * LINES; i += DCTSIZE2) {
    HWY_DYNAMIC_DISPATCH(quantize_fwd)(in[0] + i, qtableL);
  }
  for (int c = 1; c < nc; ++c) {
    for (int i = 0; i < width / scale_x * LINES / scale_y; i += DCTSIZE2) {
      HWY_DYNAMIC_DISPATCH(quantize_fwd)(in[c] + i, qtableC);
    }
  }
}
}  // namespace jpegenc_hwy

void create_qtable(int c, int QF, int *qtable) {
  float scale = (QF < 50) ? 5000.0F / static_cast<float>(QF) : 200.0F - 2.0F * static_cast<float>(QF);
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
    int val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0F) {
      stepsize = 1.0F;
    }
    if (stepsize > 255.0F) {
      stepsize = 255.0F;
    }
    // val = static_cast<int>(lround((qscale[i] / stepsize) * (1 << 16)));
    val = static_cast<int>((qscale[i] / stepsize) * (1 << 16) + 0.5);
    //     val       = (val > 0x7FFFU) ? 0x7FFF : val;
    qtable[i] = val;
  }
}

#endif