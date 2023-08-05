// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "block_coding.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include <utility>

#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void EncodeSingleBlock(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
  int dc  = sp[0];
  sp[0]   = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc = dc;
  uint64_t bitmap;
#if HWY_TARGET != HWY_SCALAR
  HWY_ALIGN int16_t dp[64];
  HWY_ALIGN uint8_t bits[64];
  #if HWY_MAX_BYTES == 64
    #include "block_coding_512.cpp"
  #elif HWY_MAX_BYTES == 32
    #include "block_coding_256.cpp"
  #else
    #include "block_coding_128.cpp"
  #endif
#else
  #include "block_coding_scalar.cpp"
#endif
}

HWY_ATTR void make_zigzag_blk(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                              huff_info &tab_Y, huff_info &tab_C, bitstream &enc) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int stride;
  int16_t *sp0, *sp1, *sp2;

  stride = width * DCTSIZE;
  if (nc == 3) {
    sp1 = in[1];
    sp2 = in[2];
    for (int Ly = 0, Cy = 0; Ly < LINES / DCTSIZE; Ly += Vl, ++Cy) {
      for (int Lx = 0, Cx = 0; Lx < width / DCTSIZE; Lx += Hl, ++Cx) {
        // Luma, Y
        for (int y = 0; y < Vl; ++y) {
          for (int x = 0; x < Hl; ++x) {
            sp0 = in[0] + (Ly + y) * stride + (Lx + x) * DCTSIZE2;  // top-left of an MCU
            EncodeSingleBlock(sp0, tab_Y, prev_dc[0], enc);
          }
        }
        // Chroma, Cb
        EncodeSingleBlock(sp1, tab_C, prev_dc[1], enc);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        EncodeSingleBlock(sp2, tab_C, prev_dc[2], enc);
        sp2 += DCTSIZE2;
      }
    }
  } else {
    sp0 = in[0];
    for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        EncodeSingleBlock(sp0, tab_Y, prev_dc[0], enc);
        sp0 += DCTSIZE2;
      }
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(make_zigzag_blk);
void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 huff_info &tab_Y, huff_info &tab_C, bitstream &enc) {
  HWY_DYNAMIC_DISPATCH(make_zigzag_blk)(std::move(in), width, YCCtype, prev_dc, tab_Y, tab_C, enc);
}
}  // namespace jpegenc_hwy
#endif