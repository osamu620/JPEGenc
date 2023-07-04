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

static inline void EncodeDC(int diff, const unsigned int *Ctable, const unsigned int *Ltable,
                            bitstream &enc) {
  //  int uval = (diff < 0) ? -diff : diff;
  uint32_t uval = (diff + (diff >> 31)) ^ (diff >> 31);
#if not defined(JPEG_USE_NEON)
  int s     = 0;
  int bound = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
#else
  int s = 32 - __builtin_clz(uval);
#endif
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0) {
    diff -= (diff >> 31) & 1;
    //    if (diff < 0) {
    //      diff -= 1;
    //      // diff = (~(0xFFFFFFFFU << s)) & (diff - 1);
    //    }
    enc.put_bits(diff, s);
  }
}

static inline void EncodeAC(int run, int val, const unsigned int *Ctable, const unsigned int *Ltable,
                            bitstream &enc) {
  //  int uval = (val < 0) ? -val : val;
  uint32_t uval = (val + (val >> 31)) ^ (val >> 31);
#if not defined(JPEG_USE_NEON)
  int s     = 0;
  int bound = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
#else
  int s = 32 - __builtin_clz(uval);
#endif
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0) {
    val -= (val >> 31) & 1;
    //    if (val < 0) {
    //      val -= 1;
    //      // val = (~(0xFFFFFFFFU << s)) & (val - 1);
    //    }
    enc.put_bits(val, s);
  }
}

static inline void encode_blk(const int16_t *in, int c, int &prev_dc, bitstream &enc) {
  int run  = 0;
  int diff = in[0] - prev_dc;
  prev_dc  = in[0];
  EncodeDC(diff, DC_cwd[c], DC_len[c], enc);

#if defined(JPEG_USE_NEON)
  uint64_t bitmap = 0;
  int16x8x4_t data1, data2;
  uint16x8_t chunk;
  uint16x4_t high_bits;
  uint32x2_t paired16;
  uint64x1_t paired32;
  uint8x8_t paired64;
  uint16x8_t equalMask;
  uint8x8_t res;
  for (int i = 0; i < 4; ++i) {
    data1.val[i] = vld1q_s16(in + 8 * i);
  }
  for (int i = 0; i < 4; ++i) {
    data2.val[i] = vld1q_s16(in + 32 + 8 * i);
  }
  for (int i = 0; i < 4; ++i) {
    bitmap <<= 8;
    chunk = data1.val[i];
    // vreinterpretq_u16_u8(vceqq_u8(chunk, vdupq_n_u8(0x00)));
    equalMask = vceqq_u16(chunk, vdupq_n_u16(0x0000));
    res       = vshrn_n_u16(equalMask, 4);
    high_bits = vreinterpret_u16_u8(vshr_n_u8(vrev64_u8(res), 7));
    paired16  = vreinterpret_u32_u16(vsra_n_u16(high_bits, high_bits, 7));
    paired32  = vreinterpret_u64_u32(vsra_n_u32(paired16, paired16, 14));
    paired64  = vreinterpret_u8_u64(vsra_n_u64(paired32, paired32, 28));
    bitmap |= vget_lane_u8(paired64, 0) | ((int)vget_lane_u8(paired64, 4) << 4);
  }
  for (int i = 0; i < 4; ++i) {
    bitmap <<= 8;
    chunk     = data2.val[i];
    equalMask = vceqq_u16(chunk, vdupq_n_u16(0x0000));
    res       = vshrn_n_u16(equalMask, 4);
    high_bits = vreinterpret_u16_u8(vshr_n_u8(vrev64_u8(res), 7));
    paired16  = vreinterpret_u32_u16(vsra_n_u16(high_bits, high_bits, 7));
    paired32  = vreinterpret_u64_u32(vsra_n_u32(paired16, paired16, 14));
    paired64  = vreinterpret_u8_u64(vsra_n_u64(paired32, paired32, 28));
    bitmap |= vget_lane_u8(paired64, 0) | ((int)vget_lane_u8(paired64, 4) << 4);
    //    matches    = vget_lane_u64(vreinterpret_u64_u8(res), 0);
    //    auto vvv   = vshr_n_u8(vcnt_u8((uint8x8_t)matches), 3);
    //    uint64_t v = (uint64_t)vvv;
    //    for (int j = 0; j < 8; ++j) {
    //      bitmap <<= 1;
    //      bitmap |= v & 0xFF;
    //      v >>= 8;
    //    }
  }

  bitmap        = ~bitmap;
  const int idx = 64 - __builtin_ctzll(bitmap);
  bool haveEOB  = true;
  if (idx == 64) {
    haveEOB = false;
  }
  bitmap <<= 1;

  int count = 1;
  while (count <= idx) {
    int r = __builtin_clzll(bitmap);
    if (r > 0) {
      count += r;
      bitmap <<= r;
      run = r;
    } else {
      while (run > 15) {
        EncodeAC(0xF, 0x0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      EncodeAC(run, in[count], AC_cwd[c], AC_len[c], enc);
      run = 0;
      count++;
      bitmap <<= 1;
    }
  }
  if (haveEOB) {
    EncodeAC(0x0, 0x0, AC_cwd[c], AC_len[c], enc);
  }
#else
  int ac;
  for (int i = 1; i < 64; ++i) {
    ac = in[i];
    if (ac == 0) {
      run++;
    } else {
      while (run > 15) {
        // ZRL
        EncodeAC(0xF, 0x0, AC_cwd[c], AC_len[c], enc);
        run -= 16;
      }
      EncodeAC(run, ac, AC_cwd[c], AC_len[c], enc);
      run = 0;
    }
  }
  if (run) {
    // EOB
    EncodeAC(0x0, 0x0, AC_cwd[c], AC_len[c], enc);
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
  for (int Ly = 0, Cy = 0; Ly < LINES; Ly += DCTSIZE * Vl, Cy += DCTSIZE) {
    for (int Lx = 0, Cx = 0; Lx < width; Lx += DCTSIZE * Hl, Cx += DCTSIZE) {
      // Encoding an MCU
      // Luma, Y
      for (int y = 0; y < Vl; ++y) {
        for (int x = 0; x < Hl; ++x) {
          encode_blk(sp0, 0, prev_dc[0], enc);
          sp0 += DCTSIZE * DCTSIZE;
        }
      }
      if (in.size() == 3) {
        // Chroma, Cb
        encode_blk(sp1, 1, prev_dc[1], enc);
        sp1 += DCTSIZE * DCTSIZE;
        // Chroma, Cr
        encode_blk(sp2, 1, prev_dc[2], enc);
        sp2 += DCTSIZE * DCTSIZE;
      }
    }
  }
}