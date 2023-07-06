#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"
#include "zigzag_order.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
#endif

void construct_MCUs(std::vector<int16_t *> in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc = in.size();
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int stride;
  int16_t *sp, *dp;
  auto make_zigzag_blk = [](const int16_t *sp, int16_t *&dp, int stride) {
    for (int i = 0; i < DCTSIZE * DCTSIZE; ++i) {
      *dp++ = sp[scan[i]];
    }
  };
  // Luma, Y
  stride = width * DCTSIZE;
  dp     = out[0];
  for (int Ly = 0; Ly < LINES / DCTSIZE; Ly += Vl) {
    for (int Lx = 0; Lx < width / DCTSIZE; Lx += Hl) {
      for (int y = 0; y < Vl; ++y) {
        for (int x = 0; x < Hl; ++x) {
          sp = in[0] + (Ly + y) * stride + (Lx + x) * 64;  // top-left of a block
          make_zigzag_blk(sp, dp, stride);
        }
      }
    }
  }
  // Chroma, Cb and Cr
  stride = width * DCTSIZE / Hl;
  for (int c = 1; c < nc; ++c) {
    dp = out[c];
    for (int Cy = 0; Cy < LINES / DCTSIZE / Vl; ++Cy) {
      for (int Cx = 0; Cx < width / DCTSIZE / Hl; ++Cx) {
        sp = in[c] + Cy * stride + Cx * 64;  // top-left of a block
        make_zigzag_blk(sp, dp, stride);
      }
    }
  }
}

static FORCE_INLINE void EncodeDC(int val, int16_t s, const unsigned int *Ctable,
                                  const unsigned int *Ltable, bitstream &enc) {
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0) {
#if not defined(JPEG_USE_NEON)
    //    if (val < 0) {
    //      val -= 1;
    //    }
    val -= (val >> 31) & 1;
#endif
    enc.put_bits(val, s);
  }
}

static FORCE_INLINE void EncodeAC(int run, int val, int16_t s, const unsigned int *Ctable,
                                  const unsigned int *Ltable, bitstream &enc) {
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0) {
#if not defined(JPEG_USE_NEON)
    val -= (val >> 31) & 1;
#endif
    enc.put_bits(val, s);
  }
}

static FORCE_INLINE void encode_blk(int16_t *const in, int c, int &prev_dc, bitstream &enc) {
  int run = 0;
  int dc  = in[0];
  in[0] -= prev_dc;
  prev_dc = dc;

#if defined(JPEG_USE_NEON)
  int16_t bits[64];
  uint64_t bitmap = 0;
  uint16x8_t chunk, equalMask;
  uint16x4_t high_bits;
  uint32x2_t paired16;
  uint64x1_t paired32;
  uint8x8_t res;
  for (int i = 0; i < 8; ++i) {
    bitmap <<= 8;
    chunk                = vld1q_s16(in + 8 * i);
    int16x8_t abs_row0   = vabsq_s16(chunk);
    int16x8_t row0_lz    = vclzq_s16(abs_row0);
    uint16x8_t row0_mask = vshlq_u16(vcltzq_s16(chunk), vnegq_s16(row0_lz));
    uint16x8_t row0_diff = veorq_u16(vreinterpretq_u16_s16(abs_row0), row0_mask);
    vst1q_s16(bits + i * 8, vsubq_u16(vdupq_n_u16(16), row0_lz));
    vst1q_s16(in + 8 * i, row0_diff);
    equalMask = vceqq_u16(chunk, vdupq_n_u16(0x0000));
    res       = vshrn_n_u16(equalMask, 4);
    high_bits = vreinterpret_u16_u8(vshr_n_u8(vrev64_u8(res), 7));
    paired16  = vreinterpret_u32_u16(vsra_n_u16(high_bits, high_bits, 7));
    paired32  = vreinterpret_u64_u32(vsra_n_u32(paired16, paired16, 14));
    res       = vreinterpret_u8_u64(vsra_n_u64(paired32, paired32, 28));
    bitmap |= vget_lane_u8(res, 0) | ((int)vget_lane_u8(res, 4) << 4);
  }

  bitmap             = ~bitmap;
  const int idx      = 64 - __builtin_ctzll(bitmap);
  const bool haveEOB = (idx != 64);
  bitmap <<= 1;

  EncodeDC(in[0], bits[0], DC_cwd[c], DC_len[c], enc);
  int count = 1;
  while (count <= idx) {
    int r = __builtin_clzll(bitmap);
    if (r > 0) {
      count += r;
      bitmap <<= r;
      run = r;
    } else {
      while (run > 15) {
        EncodeAC(0xF, 0x0, 0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      EncodeAC(run, in[count], bits[count], AC_cwd[c], AC_len[c], enc);
      run = 0;
      count++;
      bitmap <<= 1;
    }
  }
  if (haveEOB) {
    EncodeAC(0x0, 0x0, 0, AC_cwd[c], AC_len[c], enc);
  }
#else
  //  Branchless abs:
  //  https://stackoverflow.com/questions/9772348/get-absolute-value-without-using-abs-function-nor-if-statement
  uint32_t uval = (in[0] + (in[0] >> 31)) ^ (in[0] >> 31);
  int16_t s     = 0;
  int bound     = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
  EncodeDC(in[0], s, DC_cwd[c], DC_len[c], enc);
  int ac;
  for (int i = 1; i < 64; ++i) {
    ac = in[i];
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
#endif
}

void encode_MCUs(std::vector<int16_t *> in, int width, int YCCtype, std::vector<int> &prev_dc,
                 bitstream &enc) {
  int Hl       = YCC_HV[YCCtype][0] >> 4;
  int Vl       = YCC_HV[YCCtype][0] & 0xF;
  int16_t *sp0 = in[0], *sp1, *sp2;
  if (in.size() == 3) {
    sp1 = in[1];
    sp2 = in[2];
  }
  // Construct MCUs
  constexpr size_t len  = DCTSIZE * DCTSIZE;
  const size_t num_mcus = width * LINES / (DCTSIZE * DCTSIZE * Vl * Hl);
  for (size_t n = num_mcus; n > 0; --n) {
    // Encoding an MCU
    // Luma, Y
    for (int i = Vl * Hl; i > 0; --i) {
      encode_blk(sp0, 0, prev_dc[0], enc);
      sp0 += len;
    }
    if (in.size() == 3) {
      // Chroma, Cb
      encode_blk(sp1, 1, prev_dc[1], enc);
      sp1 += len;
      // Chroma, Cr
      encode_blk(sp2, 1, prev_dc[2], enc);
      sp2 += len;
    }
  }
}