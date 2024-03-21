// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "quantization.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include <cmath>
#include "quantization.hpp"
#include "constants.hpp"

HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

void quantize_core(int16_t *HWY_RESTRICT data, const int16_t *HWY_RESTRICT qtable) {
#if HWY_TARGET != HWY_SCALAR
  const hn::ScalableTag<int16_t> d16;
  for (int i = 0; i < DCTSIZE2; i += Lanes(d16)) {
    auto q = Load(d16, qtable + i);
    auto v = Load(d16, data + i);
    //    auto vl = PromoteLowerTo(d32, v);
    //    auto vh = PromoteUpperTo(d32, v);
    //
    //    vl = MulAdd(vl, ql, half);
    //    vh = MulAdd(vh, qh, half);
    //    vl = hn::ShiftRight<16>(vl);
    //    vh = hn::ShiftRight<16>(vh);
    //    Stream(OrderedDemote2To(d16, vl, vh), d16, data + i);
    v = MulFixedPoint15(v, q);
    Store(v, d16, data + i);
  }
#else
  int shift = 15;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < DCTSIZE2; ++i) {
    data[i] = static_cast<int16_t>(((int32_t)data[i] * qtable[i] + half) >> shift);
  }
#endif
}

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

void create_scaled_qtable(int c, int QF, int16_t *qtable) {
  float scale = (QF < 50) ? 5000.0F / static_cast<float>(QF) : 200.0F - 2.0F * static_cast<float>(QF);
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
    int16_t val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0F) {
      stepsize = 1.0F;
    }
    if (stepsize > 255.0F) {
      stepsize = 255.0F;
    }
    val       = static_cast<int16_t>(lround((qscale[i] / stepsize) * (1 << 15)));
    qtable[i] = val;
  }
}

#endif