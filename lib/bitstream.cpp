// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "bitstream.cpp"  // this file
#include <hwy/foreach_target.h>             // must come before highway.h
#include <hwy/highway.h>

#include "bitstream.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void trial(uint8_t *HWY_RESTRICT in, uint8_t *HWY_RESTRICT out) {
#if HWY_TARGET != HWY_SCALAR
  HWY_CAPPED(uint8_t, 8) u8;
  auto vin = Load(u8, in);
  vin      = Reverse(u8, vin);
  Store(vin, u8, out);
#else
  for (int i = 7; i >= 0; --i) {
    *out++ = in[i];
  }
#endif
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(trial);
void send_8_bytes(uint8_t *in, uint8_t *out) {
  HWY_DYNAMIC_DISPATCH(trial)
  (in, out);
}
}  // namespace jpegenc_hwy
#endif