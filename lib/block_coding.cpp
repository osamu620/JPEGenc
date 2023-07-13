#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
// clang-format off
// This table is borrowed from libjpeg-turbo
alignas(16) const uint8_t jsimd_huff_encode_one_block_consts[] = {
        0,   1,   2,   3,  16,  17,  32,  33,
        18,  19,   4,   5,   6,   7,  20,  21,
        34,  35,  48,  49, 255, 255,  50,  51,
        36,  37,  22,  23,   8,   9,  10,  11,
        255, 255,   6,   7,  20,  21,  34,  35,
        48,  49, 255, 255,  50,  51,  36,  37,
        54,  55,  40,  41,  26,  27,  12,  13,
        14,  15,  28,  29,  42,  43,  56,  57,
        6,   7,  20,  21,  34,  35,  48,  49,
        50,  51,  36,  37,  22,  23,   8,   9,
        26,  27,  12,  13, 255, 255,  14,  15,
        28,  29,  42,  43,  56,  57, 255, 255,
        52,  53,  54,  55,  40,  41,  26,  27,
        12,  13, 255, 255,  14,  15,  28,  29,
        26,  27,  40,  41,  42,  43,  28,  29,
        14,  15,  30,  31,  44,  45,  46,  47
};
// clang-format on

#else
  #include "zigzag_order.hpp"
static FORCE_INLINE void EncodeDC(int val, int16_t s, const unsigned int *Ctable, const int *Ltable,
                                  bitstream &enc) {
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0) {
    //    if (val < 0) {
    //      val -= 1;
    //    }
    val -= (val >> 31) & 1;
    enc.put_bits(val, s);
  }
}

static FORCE_INLINE void EncodeAC(int run, int val, int16_t s, const unsigned int *Ctable,
                                  const int *Ltable, bitstream &enc) {
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0) {
    val -= (val >> 31) & 1;
    enc.put_bits(val, s);
  }
}
#endif

static void make_zigzag_blk(int16_t *sp, int c, int &prev_dc, bitstream &enc) {
  alignas(16) int16_t dp[64];
  int dc  = sp[0];
  sp[0]   = static_cast<int16_t>(sp[0] - prev_dc);
  prev_dc = dc;
#if not defined(JPEG_USE_NEON)
  for (int i = 0; i < DCTSIZE2; ++i) {
    dp[i] = sp[scan[i]];
  }
  int run = 0;
  //  Branchless abs:
  //  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
  uint32_t uval = (dp[0] + (dp[0] >> 31)) ^ (dp[0] >> 31);
  int16_t s     = 0;
  int bound     = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
  EncodeDC(dp[0], s, DC_cwd[c], DC_len[c], enc);
  int ac;
  for (int i = 1; i < 64; ++i) {
    ac = dp[i];
    if (ac == 0) {
      run++;
    } else {
      while (run > 15) {
        // ZRL
        EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      s     = 0;
      bound = 1;
      uval  = (ac + (ac >> 31)) ^ (ac >> 31);
      while (uval >= bound) {
        bound += bound;
        s++;
      }
      EncodeAC(run, ac, s, AC_cwd[c], AC_len[c], enc);
      run = 0;
    }
  }
  if (run) {
    // EOB
    EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
  }
#else
  // This code is borrowed from libjpeg-turbo
  const uint8x16x4_t idx_rows_0123 = vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 0 * DCTSIZE);
  const uint8x16x4_t idx_rows_4567 = vld1q_u8_x4(jsimd_huff_encode_one_block_consts + 8 * DCTSIZE);

  const int8x16x4_t tbl_rows_0123 = vld1q_s8_x4((int8_t *)(sp + 0 * DCTSIZE));
  const int8x16x4_t tbl_rows_4567 = vld1q_s8_x4((int8_t *)(sp + 4 * DCTSIZE));

  /* Initialise extra lookup tables. */
  const int8x16x4_t tbl_rows_2345 = {
      {tbl_rows_0123.val[2], tbl_rows_0123.val[3], tbl_rows_4567.val[0], tbl_rows_4567.val[1]}};
  const int8x16x3_t tbl_rows_567 = {{tbl_rows_4567.val[1], tbl_rows_4567.val[2], tbl_rows_4567.val[3]}};

  /* Shuffle coefficients into zig-zag order. */
  int16x8_t row0 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[0]));
  int16x8_t row1 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[1]));
  int16x8_t row2 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_0123.val[2]));
  int16x8_t row3 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_0123, idx_rows_0123.val[3]));
  int16x8_t row4 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[0]));
  int16x8_t row5 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_2345, idx_rows_4567.val[1]));
  int16x8_t row6 = vreinterpretq_s16_s8(vqtbl4q_s8(tbl_rows_4567, idx_rows_4567.val[2]));
  int16x8_t row7 = vreinterpretq_s16_s8(vqtbl3q_s8(tbl_rows_567, idx_rows_4567.val[3]));

  /* Initialize AC coefficient lanes not reachable by lookup tables. */
  row1 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[0]), 0), row1, 2);
  row2 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 4), row2, 0);
  row2 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 0), row2, 5);
  row5 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[1]), 7), row5, 2);
  row5 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_4567.val[2]), 3), row5, 7);
  row6 = vsetq_lane_s16(vgetq_lane_s16(vreinterpretq_s16_s8(tbl_rows_0123.val[3]), 7), row6, 5);

  /* DCT block is now in zig-zag order; start Huffman encoding process. */

  /* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
   * means that the corresponding coefficient != 0.
   */
  uint16x8_t row0_ne_0 = vtstq_s16(row0, row0);
  uint16x8_t row1_ne_0 = vtstq_s16(row1, row1);
  uint16x8_t row2_ne_0 = vtstq_s16(row2, row2);
  uint16x8_t row3_ne_0 = vtstq_s16(row3, row3);
  uint16x8_t row4_ne_0 = vtstq_s16(row4, row4);
  uint16x8_t row5_ne_0 = vtstq_s16(row5, row5);
  uint16x8_t row6_ne_0 = vtstq_s16(row6, row6);
  uint16x8_t row7_ne_0 = vtstq_s16(row7, row7);

  uint8x16_t row10_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row1_ne_0), vreinterpretq_u8_u16(row0_ne_0));
  uint8x16_t row32_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row3_ne_0), vreinterpretq_u8_u16(row2_ne_0));
  uint8x16_t row54_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row5_ne_0), vreinterpretq_u8_u16(row4_ne_0));
  uint8x16_t row76_ne_0 = vuzp1q_u8(vreinterpretq_u8_u16(row7_ne_0), vreinterpretq_u8_u16(row6_ne_0));

  /* { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } */
  const uint8x16_t bitmap_mask = vreinterpretq_u8_u64(vdupq_n_u64(0x0102040810204080));

  uint8x16_t bitmap_rows_10 = vandq_u8(row10_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_32 = vandq_u8(row32_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_54 = vandq_u8(row54_ne_0, bitmap_mask);
  uint8x16_t bitmap_rows_76 = vandq_u8(row76_ne_0, bitmap_mask);

  uint8x16_t bitmap_rows_3210     = vpaddq_u8(bitmap_rows_32, bitmap_rows_10);
  uint8x16_t bitmap_rows_7654     = vpaddq_u8(bitmap_rows_76, bitmap_rows_54);
  uint8x16_t bitmap_rows_76543210 = vpaddq_u8(bitmap_rows_7654, bitmap_rows_3210);
  uint8x8_t bitmap_all = vpadd_u8(vget_low_u8(bitmap_rows_76543210), vget_high_u8(bitmap_rows_76543210));
  /* Move bitmap to 64-bit scalar register. */
  uint64_t bitmap = vget_lane_u64(vreinterpret_u64_u8(bitmap_all), 0);
  uint8_t bits[64];

  int16x8_t abs_row0 = vabsq_s16(row0);
  int16x8_t abs_row1 = vabsq_s16(row1);
  int16x8_t abs_row2 = vabsq_s16(row2);
  int16x8_t abs_row3 = vabsq_s16(row3);
  int16x8_t abs_row4 = vabsq_s16(row4);
  int16x8_t abs_row5 = vabsq_s16(row5);
  int16x8_t abs_row6 = vabsq_s16(row6);
  int16x8_t abs_row7 = vabsq_s16(row7);

  int16x8_t row0_lz = vclzq_s16(abs_row0);
  int16x8_t row1_lz = vclzq_s16(abs_row1);
  int16x8_t row2_lz = vclzq_s16(abs_row2);
  int16x8_t row3_lz = vclzq_s16(abs_row3);
  int16x8_t row4_lz = vclzq_s16(abs_row4);
  int16x8_t row5_lz = vclzq_s16(abs_row5);
  int16x8_t row6_lz = vclzq_s16(abs_row6);
  int16x8_t row7_lz = vclzq_s16(abs_row7);

  /* Narrow leading zero count to 8 bits. */
  uint8x16_t row01_lz = vuzp1q_u8(vreinterpretq_u8_s16(row0_lz), vreinterpretq_u8_s16(row1_lz));
  uint8x16_t row23_lz = vuzp1q_u8(vreinterpretq_u8_s16(row2_lz), vreinterpretq_u8_s16(row3_lz));
  uint8x16_t row45_lz = vuzp1q_u8(vreinterpretq_u8_s16(row4_lz), vreinterpretq_u8_s16(row5_lz));
  uint8x16_t row67_lz = vuzp1q_u8(vreinterpretq_u8_s16(row6_lz), vreinterpretq_u8_s16(row7_lz));
  /* Compute nbits needed to specify magnitude of each coefficient. */
  uint8x16_t row01_nbits = vsubq_u8(vdupq_n_u8(16), row01_lz);
  uint8x16_t row23_nbits = vsubq_u8(vdupq_n_u8(16), row23_lz);
  uint8x16_t row45_nbits = vsubq_u8(vdupq_n_u8(16), row45_lz);
  uint8x16_t row67_nbits = vsubq_u8(vdupq_n_u8(16), row67_lz);
  /* Store nbits. */
  vst1q_u8(bits + 0 * DCTSIZE, row01_nbits);
  vst1q_u8(bits + 2 * DCTSIZE, row23_nbits);
  vst1q_u8(bits + 4 * DCTSIZE, row45_nbits);
  vst1q_u8(bits + 6 * DCTSIZE, row67_nbits);

  uint16x8_t row0_mask = vshlq_u16(vcltzq_s16(row0), vnegq_s16(row0_lz));
  uint16x8_t row1_mask = vshlq_u16(vcltzq_s16(row1), vnegq_s16(row1_lz));
  uint16x8_t row2_mask = vshlq_u16(vcltzq_s16(row2), vnegq_s16(row2_lz));
  uint16x8_t row3_mask = vshlq_u16(vcltzq_s16(row3), vnegq_s16(row3_lz));
  uint16x8_t row4_mask = vshlq_u16(vcltzq_s16(row4), vnegq_s16(row4_lz));
  uint16x8_t row5_mask = vshlq_u16(vcltzq_s16(row5), vnegq_s16(row5_lz));
  uint16x8_t row6_mask = vshlq_u16(vcltzq_s16(row6), vnegq_s16(row6_lz));
  uint16x8_t row7_mask = vshlq_u16(vcltzq_s16(row7), vnegq_s16(row7_lz));

  uint16x8_t row0_diff = veorq_u16(vreinterpretq_u16_s16(abs_row0), row0_mask);
  uint16x8_t row1_diff = veorq_u16(vreinterpretq_u16_s16(abs_row1), row1_mask);
  uint16x8_t row2_diff = veorq_u16(vreinterpretq_u16_s16(abs_row2), row2_mask);
  uint16x8_t row3_diff = veorq_u16(vreinterpretq_u16_s16(abs_row3), row3_mask);
  uint16x8_t row4_diff = veorq_u16(vreinterpretq_u16_s16(abs_row4), row4_mask);
  uint16x8_t row5_diff = veorq_u16(vreinterpretq_u16_s16(abs_row5), row5_mask);
  uint16x8_t row6_diff = veorq_u16(vreinterpretq_u16_s16(abs_row6), row6_mask);
  uint16x8_t row7_diff = veorq_u16(vreinterpretq_u16_s16(abs_row7), row7_mask);

  vst1q_s16(dp + 0 * DCTSIZE, row0_diff);
  vst1q_s16(dp + 1 * DCTSIZE, row1_diff);
  vst1q_s16(dp + 2 * DCTSIZE, row2_diff);
  vst1q_s16(dp + 3 * DCTSIZE, row3_diff);
  vst1q_s16(dp + 4 * DCTSIZE, row4_diff);
  vst1q_s16(dp + 5 * DCTSIZE, row5_diff);
  vst1q_s16(dp + 6 * DCTSIZE, row6_diff);
  vst1q_s16(dp + 7 * DCTSIZE, row7_diff);

  bitmap <<= 1;

  // EncodeDC(dp[0], bits[0], DC_cwd[c], DC_len[c], enc);
  enc.put_bits(DC_cwd[c][bits[0]], DC_len[c][bits[0]]);
  if (bits[0] != 0) {
    enc.put_bits(dp[0], bits[0]);
  }
  int count = 1;
  int run;
  while (bitmap != 0) {
    run = __builtin_clzll(bitmap);
    count += run;
    bitmap <<= run;
    while (run > 15) {
      // EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
      enc.put_bits(AC_cwd[c][0xF0], AC_len[c][0xF0]);
      run -= 16;
    }
    // EncodeAC(run, dp[count], bits[count], AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][(run << 4) + bits[count]], AC_len[c][(run << 4) + bits[count]]);
    enc.put_bits(dp[count], bits[count]);
    count++;
    bitmap <<= 1;
  }
  if (count != 64) {
    // EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
    enc.put_bits(AC_cwd[c][0x00], AC_len[c][0x00]);
  }
#endif
}

void Encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc) {
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
            make_zigzag_blk(sp0, 0, prev_dc[0], enc);
          }
        }
        // Chroma, Cb
        make_zigzag_blk(sp1, 1, prev_dc[1], enc);
        sp1 += DCTSIZE2;
        // Chroma, Cr
        make_zigzag_blk(sp2, 1, prev_dc[2], enc);
        sp2 += DCTSIZE2;
      }
    }
  } else {
    sp0 = in[0];
    for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
      for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
        // Luma, Y
        make_zigzag_blk(sp0, 0, prev_dc[0], enc);
        sp0 += DCTSIZE2;
      }
    }
  }
}