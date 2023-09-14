// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "quantization.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include <cmath>
#include <utility>
#include "ycctype.hpp"
#include "constants.hpp"
#include "quantization.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void quantize_core(int16_t *HWY_RESTRICT data, const int *HWY_RESTRICT qtable) {
#if HWY_TARGET != HWY_SCALAR
  const hn::ScalableTag<int16_t> d16;
  const hn::ScalableTag<int32_t> d32;
  const auto half = hn::Set(d32, 1 << 15);
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

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE

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
    val = static_cast<int>(lround((qscale[i] / stepsize) * (1 << 16)));
    //    val = static_cast<int>((qscale[i] / stepsize) * (1 << 16) + 0.5);
    qtable[i] = val;
  }
}

#endif