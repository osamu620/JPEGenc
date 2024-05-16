// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "block_coding.cpp"  // this file
#include <hwy/foreach_target.h>                // must come before highway.h
#include <hwy/highway.h>

#include <cstring>
#include <cmath>

#include "block_coding.hpp"
#include "ycctype.hpp"

HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy {
namespace HWY_NAMESPACE {

#include "dct.cpp"
#include "quantization.cpp"

void encode_block(int16_t *HWY_RESTRICT sp, huff_info &tab, int &prev_dc, bitstream &enc) {
  uint64_t bitmap;
  HWY_ALIGN int16_t dp[64];
#if HWY_TARGET != HWY_SCALAR
  HWY_ALIGN uint8_t bits[64];

  #if HWY_TARGET <= HWY_AVX3
    #include "block_coding_512.cpp"
  #elif HWY_TARGET <= HWY_AVX2
    #include "block_coding_256.cpp"
  #else
    #include "block_coding_128.cpp"
  #endif

  // update previous dc value
  prev_dc = sp[0];

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

  uint32_t diff;
  int32_t nbits;
  while (bitmap != 0) {
    int run = JPEGENC_CLZ64(bitmap);
    count += run;
    bitmap <<= run;
    diff  = dp[count];
    nbits = bits[count];
    while (run > 15) {
      // ZRL
      enc.put_bits(ZRL_cwd, ZRL_len);
      run -= 16;
    }
    // EncodeAC
    int32_t RS = (run << 4) + nbits;  //    size_t RS = (run << 4) + bits[count];
    diff |= tab.AC_cwd[RS] << nbits;  //    enc.put_bits(tab.AC_cwd[RS], tab.AC_len[RS]);
    nbits += tab.AC_len[RS];          //    enc.put_bits(dp[count], bits[count]);
    enc.put_bits(diff, nbits);

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

void encode_mcus(std::vector<int16_t *> &in, int width, const int mcu_height, const int YCCtype,
                 const int16_t *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &tab_Y,
                 huff_info &tab_C, bitstream &enc) {
  const int nc       = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  const int num_mcus = (mcu_height / (DCTSIZE)) * (width / (DCTSIZE));
  const int mcu_skip = (YCC_HV[YCCtype][0] >> 4) * (YCC_HV[YCCtype][0] & 0xF);

  int16_t *block0 = in[0];
  int16_t *block1 = in[1];
  int16_t *block2 = in[2];

  if (nc == 3) {  // color
    for (int k = 0; k < num_mcus; k += mcu_skip) {
      // DCT, Quantization
      for (int i = mcu_skip; i > 0; --i) {
        dct2_core(block0);
        block0 += DCTSIZE2;
      }
      dct2_core(block1);
      dct2_core(block2);

      block0 = in[0] + k * DCTSIZE2;

      for (int i = mcu_skip; i > 0; --i) {
        quantize_core(block0, qtable);
        block0 += DCTSIZE2;
      }
      quantize_core(block1, qtable + DCTSIZE2);
      quantize_core(block2, qtable + DCTSIZE2);

      block0 = in[0] + k * DCTSIZE2;

      // Huffman-coding
      for (int i = mcu_skip; i > 0; --i) {
        encode_block(block0, tab_Y, prev_dc[0], enc);
        block0 += DCTSIZE2;
      }
      encode_block(block1, tab_C, prev_dc[1], enc);
      encode_block(block2, tab_C, prev_dc[2], enc);

      block1 += DCTSIZE2;
      block2 += DCTSIZE2;
    }
  } else {  // monochrome
    for (int k = 0; k < num_mcus; k += mcu_skip * 2) {
      // Process two blocks within a single iteration for the speed
      dct2_core(block0);
      dct2_core(block0 + DCTSIZE2);
      quantize_core(block0, qtable);
      quantize_core(block0 + DCTSIZE2, qtable);
      encode_block(block0, tab_Y, prev_dc[0], enc);
      encode_block(block0 + DCTSIZE2, tab_Y, prev_dc[0], enc);
      block0 += DCTSIZE2 * 2;
    }
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace jpegenc_hwy {
HWY_EXPORT(encode_mcus);
void encode_lines(std::vector<int16_t *> &in, int width, int mcu_height, int YCCtype,
                  int16_t *HWY_RESTRICT qtable, std::vector<int> &prev_dc, huff_info &tab_Y,
                  huff_info &tab_C, bitstream &enc) {
  HWY_DYNAMIC_DISPATCH(encode_mcus)
  (in, width, mcu_height, YCCtype, qtable, prev_dc, tab_Y, tab_C, enc);
}
}  // namespace jpegenc_hwy
#endif