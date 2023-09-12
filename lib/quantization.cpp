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
  auto half = hn::Set(d32, 1 << 15);
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

void quantize_fwd(std::vector<int16_t *> in, const int width, const int mcu_height, const int YCCtype,
                  const int *HWY_RESTRICT qtableL, const int *HWY_RESTRICT qtableC) {
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int16_t *sp0, *sp1, *sp2;

  sp0 = in[0];
  if (nc == 3) {  // color
    sp1 = in[1];
    sp2 = in[2];
    for (int Ly = 0; Ly < mcu_height / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        for (int i = Hl * Vl; i > 0; --i) {
          quantize_core(sp0, qtableL);
          sp0 += DCTSIZE2;
        }
        // Chroma, Cb
        quantize_core(sp1, qtableC);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        quantize_core(sp2, qtableC);
        sp2 += DCTSIZE2;
      }
    }
  } else {  // monochrome
    for (int Ly = 0; Ly < mcu_height / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        quantize_core(sp0, qtableL);
        sp0 += DCTSIZE2;
      }
    }
  }
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE

namespace jpegenc_hwy {
HWY_EXPORT(quantize_fwd);
void quantize(std::vector<int16_t *> in, int width, int mcu_height, int YCCtype, int *qtableL,
              int *qtableC) {
  HWY_DYNAMIC_DISPATCH(quantize_fwd)(std::move(in), width, mcu_height, YCCtype, qtableL, qtableC);
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
    val = static_cast<int>(lround((qscale[i] / stepsize) * (1 << 16)));
    //    val = static_cast<int>((qscale[i] / stepsize) * (1 << 16) + 0.5);
    qtable[i] = val;
  }
}

#endif