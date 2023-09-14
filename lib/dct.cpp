// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dct.cpp"  // this file
#include <hwy/foreach_target.h>       // must come before highway.h
#include <hwy/highway.h>

#include <utility>

#include "dct.hpp"
#include "constants.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void fast_dct2(std::vector<int16_t *> &in, int width, int mcu_height, int YCCtype) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int16_t *sp0, *sp1, *sp2;

  sp0 = in[0];
  if (nc == 3) {  // color
    sp1 = in[1];
    sp2 = in[2];
    for (int Ly = 0; Ly < mcu_height / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        for (int i = Hl * Vl; i > 0; --i) {
          //          dct2_core(sp0);
          sp0 += DCTSIZE2;
        }
        // Chroma, Cb
        //        dct2_core(sp1);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        //        dct2_core(sp2);
        sp2 += DCTSIZE2;
      }
    }
  } else {  // monochrome
    for (int Ly = 0; Ly < mcu_height / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        //        dct2_core(sp0);
        sp0 += DCTSIZE2;
      }
    }
  }
}
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(fast_dct2);
void dct2(std::vector<int16_t *> &in, int width, int mcu_height, int YCCtype) {
  HWY_DYNAMIC_DISPATCH(fast_dct2)(in, width, mcu_height, YCCtype);
}

}  // namespace jpegenc_hwy
#endif