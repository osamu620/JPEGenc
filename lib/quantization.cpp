#include <hwy/highway.h>

#include "constants.hpp"

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