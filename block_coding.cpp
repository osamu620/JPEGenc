#include "block_coding.hpp"
#include "constants.hpp"
#include "huffman_tables.hpp"
#include "ycctype.hpp"
#include "zigzag_order.hpp"

void make_zigzag_buffer(std::vector<int16_t *> in, std::vector<int16_t *> out, int width, int YCCtype) {
  int nc = in.size();
  int Hl = YCC_HV[YCCtype][0] >> 4;
  int Vl = YCC_HV[YCCtype][0] & 0xF;
  int stride;
  int16_t *sp, *dp;
  auto make_zigzag_blk = [](const int16_t *sp, int16_t *&dp, int stride) {
    for (int i = 0; i < DCTSIZE; ++i) {
      for (int j = 0; j < DCTSIZE; ++j) {
        *dp++ = sp[z_stride[i * DCTSIZE + j] * stride + z_plus[i * DCTSIZE + j]];
      }
    }
  };
  // Y
  stride = width;
  dp     = out[0];
  for (int Ly = 0; Ly < LINES; Ly += DCTSIZE * Vl) {
    for (int Lx = 0; Lx < stride; Lx += DCTSIZE * Hl) {
      for (int y = 0; y < Vl; ++y) {
        for (int x = 0; x < Hl; ++x) {
          sp = in[0] + Ly * stride + Lx + y * DCTSIZE * stride + x * DCTSIZE;  // top-left of a block
          make_zigzag_blk(sp, dp, stride);
        }
      }
    }
  }
  // Cb, Cr
  stride = width / Hl;
  for (int c = 1; c < nc; ++c) {
    dp = out[c];
    for (int Cy = 0; Cy < LINES / Vl; Cy += DCTSIZE) {
      for (int Cx = 0; Cx < stride; Cx += DCTSIZE) {
        sp = in[c] + Cy * stride + Cx;  // top-left of a block
        make_zigzag_blk(sp, dp, stride);
      }
    }
  }
}

static inline void EncodeDC(int diff, const unsigned int *Ctable, const unsigned int *Ltable,
                            bitstream &enc) {
  int uval  = (diff < 0) ? -diff : diff;
  int s     = 0;
  int bound = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
  enc.put_bits(Ctable[s], Ltable[s]);
  if (s != 0) {
    if (diff < 0) {
      diff -= 1;
      // diff = (~(0xFFFFFFFFU << s)) & (diff - 1);
    }
    enc.put_bits(diff, s);
  }
}

static inline void EncodeAC(int run, int val, const unsigned int *Ctable, const unsigned int *Ltable,
                            bitstream &enc) {
  int uval  = (val < 0) ? -val : val;
  int s     = 0;
  int bound = 1;
  while (uval >= bound) {
    bound += bound;
    s++;
  }
  //  int s    = 32 - __builtin_clz(uval);
  enc.put_bits(Ctable[(run << 4) + s], Ltable[(run << 4) + s]);
  if (s != 0) {
    if (val < 0) {
      val -= 1;
      // val = (~(0xFFFFFFFFU << s)) & (val - 1);
    }
    enc.put_bits(val, s);
  }
}

static inline void encode_blk(const int16_t *in, int c, int &prev_dc, bitstream &enc) {
  int run  = 0;
  int diff = in[0] - prev_dc;
  prev_dc  = in[0];
  EncodeDC(diff, DC_cwd[c], DC_len[c], enc);
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
      ..
    }
  }
}