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

#if HWY_TARGET != HWY_SCALAR
  uint64_t bitmap;
  HWY_ALIGN int16_t dp[64];
  HWY_ALIGN uint8_t bits[64];

  using namespace hn;
  const ScalableTag<int16_t> s16;
  const ScalableTag<uint16_t> u16;
  const ScalableTag<uint8_t> u8;
  const ScalableTag<uint64_t> u64;

  #if HWY_MAX_BYTES == 64
    #include "block_coding_512.cpp"
  #elif HWY_MAX_BYTES == 32
    #include "block_coding_256.cpp"
  #else
    #include "block_coding_128.cpp"
  #endif

  // EncodeDC
  enc.put_bits(tab.DC_cwd[bits[0]], tab.DC_len[bits[0]]);
  if (bitmap & 0x8000000000000000) {
    enc.put_bits(dp[0], bits[0]);
  }
  bitmap <<= 1;

  int count = 1;
  while (bitmap != 0) {
    int run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // ZRL
      enc.put_bits(tab.AC_cwd[0xF0], tab.AC_len[0xF0]);
      run -= 16;
    }
    // EncodeAC
    size_t RS = (run << 4) + bits[count];
    enc.put_bits(tab.AC_cwd[RS], tab.AC_len[RS]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EOB
    enc.put_bits(tab.AC_cwd[0x00], tab.AC_len[0x00]);
  }

#else
  #include "block_coding_scalar.cpp"
#endif
}

HWY_ATTR void make_zigzag_blk(std::vector<int16_t *> in, int width, const int mcu_height, const int YCCtype,
                              std::vector<int> &prev_dc, huff_info &tab_Y, huff_info &tab_C,
                              bitstream &enc) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int stride;
  int16_t *sp0, *sp1, *sp2;

  stride = round_up(width, DCTSIZE * Hl) * DCTSIZE;
  if (width % DCTSIZE) {
    width = round_up(width, DCTSIZE);
  }
  if (nc == 3) {
    for (int Ly = 0, Cy = 0; Ly < mcu_height / DCTSIZE; Ly += Vl, ++Cy) {
      sp1 = in[1] + Ly * stride / Hl;
      sp2 = in[2] + Ly * stride / Hl;
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
    //    sp0 = in[0];
    for (int Ly = 0; Ly < mcu_height / DCTSIZE; Ly += Vl) {
      sp0 = in[0] + Ly * stride;
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
void Encode_MCUs(std::vector<int16_t *> in, int width, const int mcu_height, const int YCCtype,
                 std::vector<int> &prev_dc, huff_info &tab_Y, huff_info &tab_C, bitstream &enc) {
  HWY_DYNAMIC_DISPATCH(make_zigzag_blk)
  (std::move(in), width, mcu_height, YCCtype, prev_dc, tab_Y, tab_C, enc);
}
}  // namespace jpegenc_hwy
#endif