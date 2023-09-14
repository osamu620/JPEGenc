// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "block_coding.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>
#include <hwy/aligned_allocator.h>

#include <cstring>

#include "block_coding.hpp"
#include "constants.hpp"
#include "dct.hpp"
#include "huffman_tables.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void encode_sigle_block(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
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

  int count              = 1;
  const uint32_t ZRL_cwd = tab.AC_cwd[0xF0];
  const int32_t ZRL_len  = tab.AC_len[0xF0];
  const uint32_t EOB_cwd = tab.AC_cwd[0x00];
  const int32_t EOB_len  = tab.AC_len[0x00];

  while (bitmap != 0) {
    int run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // ZRL
      enc.put_bits(ZRL_cwd, ZRL_len);
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
    enc.put_bits(EOB_cwd, EOB_len);
  }

#else
  #include "block_coding_scalar.cpp"
#endif
}

HWY_ATTR void encode_mcus(std::vector<int16_t *> &in, int16_t *HWY_RESTRICT mcu, int width,
                          const int mcu_height, const int YCCtype, int *HWY_RESTRICT qtable,
                          std::vector<int> &prev_dc, huff_info &tab_Y, huff_info &tab_C, bitstream &enc) {
  int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;

  const int num_mcus = (mcu_height / (DCTSIZE)) * (width / (DCTSIZE));
  const int mcu_skip = Hl * Vl;

  int16_t *ssp0 = in[0];
  int16_t *ssp1 = in[1];
  int16_t *ssp2 = in[2];

  int16_t *dp, *wp;

  int pdc[3];
  pdc[0] = prev_dc[0];
  pdc[1] = prev_dc[1];
  pdc[2] = prev_dc[2];
  if (nc == 3) {  // color
    for (int k = 0; k < num_mcus; k += mcu_skip) {
      dp = wp = mcu;
      memcpy(dp, ssp0, sizeof(int16_t) * DCTSIZE2 * mcu_skip);
      memcpy(dp + DCTSIZE2 * mcu_skip, ssp1, sizeof(int16_t) * DCTSIZE2);
      memcpy(dp + DCTSIZE2 * mcu_skip + DCTSIZE2, ssp2, sizeof(int16_t) * DCTSIZE2);
      ssp0 += DCTSIZE2 * mcu_skip;
      ssp1 += DCTSIZE2;
      ssp2 += DCTSIZE2;
      // DCT
      for (int i = 0; i < mcu_skip; ++i) {
        dct2_core(wp + i * DCTSIZE2);
      }
      dct2_core(wp + mcu_skip * DCTSIZE2);
      dct2_core(wp + mcu_skip * DCTSIZE2 + DCTSIZE2);

      // Quantization
      for (int i = 0; i < mcu_skip; ++i) {
        quantize_core(wp + i * DCTSIZE2, qtable);
      }
      quantize_core(wp + mcu_skip * DCTSIZE2, qtable + DCTSIZE2);
      quantize_core(wp + mcu_skip * DCTSIZE2 + DCTSIZE2, qtable + DCTSIZE2);

      // Huffman-coding
      for (int i = mcu_skip; i > 0; --i) {
        encode_sigle_block(wp, tab_Y, pdc[0], enc);
        wp += DCTSIZE2;
      }
      encode_sigle_block(wp, tab_C, pdc[1], enc);
      wp += DCTSIZE2;
      encode_sigle_block(wp, tab_C, pdc[2], enc);
    }
  } else {  // monochrome
    dp = mcu;
    for (int k = 0; k < num_mcus; k += mcu_skip) {
      memcpy(dp, in[0], DCTSIZE2);
      ssp0 += DCTSIZE2;
      // Luma, Y
      dct2_core(dp);
      quantize_core(dp, qtable);
      encode_sigle_block(dp, tab_Y, pdc[0], enc);
      //      sp0 += DCTSIZE2;
    }
  }
  prev_dc[0] = pdc[0];
  prev_dc[1] = pdc[1];
  prev_dc[2] = pdc[2];
}

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(encode_mcus);
void encode_lines(std::vector<int16_t *> &in, int16_t *HWY_RESTRICT mcu, int width, int mcu_height,
                  int YCCtype, int *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &tab_Y,
                  huff_info &tab_C, bitstream &enc) {
  HWY_DYNAMIC_DISPATCH(encode_mcus)
  (in, mcu, width, mcu_height, YCCtype, qtable, prev_dc, tab_Y, tab_C, enc);
}
}  // namespace jpegenc_hwy
#endif