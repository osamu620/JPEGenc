#include <algorithm>
#include <vector>

#include "bitstream.hpp"
#include "huffman_tables.hpp"
#include "jpgmarkers.hpp"
#include "ycctype.hpp"
#include "zigzag_order.hpp"
#include "jpgheaders.hpp"
#include "quantization.hpp"
#include "constants.hpp"

void create_SOF(int P, int Y, int X, int Nf, int YCCtype, bitstream &enc) {
  enc.put_word(SOF);
  enc.put_word(8 + 3 * Nf);
  enc.put_byte(P);
  enc.put_word(Y);
  enc.put_word(X);
  enc.put_byte(Nf);

  for (int Ci = 0; Ci < Nf; ++Ci) {
    enc.put_byte(Ci);
    enc.put_byte(YCC_HV[YCCtype][Ci > 0]);
    int Tqi = 0;
    if (Ci > 0) {
      Tqi = 1;
    }
    enc.put_byte(Tqi);
  }
}

void create_SOS(int Ns, bitstream &enc) {
  enc.put_word(SOS);
  enc.put_word(6 + 2 * Ns);
  enc.put_byte(Ns);
  for (int Cs = 0; Cs < Ns; ++Cs) {
    enc.put_byte(Cs + 0);
    int Td = 0, Ta = 0;
    if (Cs > 0) {
      Td = 1;
      Ta = 1;
    }
    enc.put_byte((Td << 4) + Ta);
  }
  int Ss = 0, Se = 63, Ah = 0, Al = 0;
  enc.put_byte(Ss);
  enc.put_byte(Se);
  enc.put_byte((Ah << 4) + Al);
}

void create_DQT(int c, int16_t *qtable, bitstream &enc) {
  enc.put_word(DQT);
  int Pq = 0;  // baseline
  int Lq = 2 + (65 + 64 * Pq);
  enc.put_word(Lq);
  int Tq = 0;
  if (c > 0) {
    Tq = 1;
  }
  enc.put_byte((Pq << 4) + Tq);
  for (size_t i : indices) {
    enc.put_byte(qtable[i]);
  }
}

void create_DHT(int c, bitstream &enc) {
  int Lh, Tc, Th;
  int freq[16] = {0};

  // DC
  for (int i = 0; i < 16; ++i) {
    if (DC_len_[c][i]) {
      freq[DC_len_[c][i] - 1]++;
    }
  }
  uint8_t tmp[256] = {0};
  int idx          = 0;
  // Li
  for (int f : freq) {
    tmp[idx] = f;
    idx++;
  }
  // Vi
  for (int i = 0; i < 16; ++i) {
    if (DC_len_[c][i]) {
      tmp[idx] = i;
      idx++;
    }
  }
  enc.put_word(DHT);
  Lh = static_cast<uint8_t>(idx + 2 + 1);
  enc.put_word(Lh);
  Tc = 0;
  Th = c;
  enc.put_byte((Tc << 4) + Th);
  for (int i = 0; i < idx; ++i) {
    enc.put_byte(tmp[i]);
  }
  for (int &f : freq) {
    f = 0;
  }
  idx = 0;

  // AC
  std::vector<std::pair<int, int>> ACpair(256);
  for (int i = 0; i < 256; ++i) {
    ACpair[i].first  = AC_len_[c][i];
    ACpair[i].second = i;
  }
  std::sort(ACpair.begin(), ACpair.end());
  for (int i = 0; i < 256; ++i) {
    int first = ACpair[i].first;
    if (first) {
      freq[first - 1]++;
    }
  }
  // Li
  for (int f : freq) {
    tmp[idx] = f;
    idx++;
  }
  // Vi
  for (int i = 0; i < 256; ++i) {
    if (ACpair[i].first) {
      tmp[idx] = ACpair[i].second;
      idx++;
    }
  }

  enc.put_word(DHT);
  Lh = static_cast<uint8_t>(idx + 2 + 1);
  enc.put_word(Lh);
  Tc = 1;
  Th = c;
  enc.put_byte((Tc << 4) + Th);
  for (int i = 0; i < idx; ++i) {
    enc.put_byte(tmp[i]);
  }
}

void create_mainheader(int width, int height, int QF, int YCCtype, bitstream &enc, bool use_RESET) {
  enc.put_word(SOI);
  const int nc = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;
  int16_t qtable[64];
  auto create_qtable_DQT = [](int c, int QF, int16_t *qtable) {
    float scale = (QF < 50) ? 5000.0F / static_cast<float>(QF) : 200.0F - 2.0F * static_cast<float>(QF);
    for (int i = 0; i < DCTSIZE2; ++i) {
      float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
      stepsize       = floor(stepsize);
      if (stepsize < 1.0) {
        stepsize = 1;
      }
      if (stepsize > 255) {
        stepsize = 255;
      }
      qtable[i] = static_cast<int16_t>(stepsize);
    }
  };
  create_qtable_DQT(0, QF, qtable);
  create_DQT(0, qtable, enc);
  if (nc > 1) {
    create_qtable_DQT(1, QF, qtable);
    create_DQT(1, qtable, enc);
  }
  create_SOF(8, height, width, nc, YCCtype, enc);
  create_DHT(0, enc);
  if (nc > 1) {
    create_DHT(1, enc);
  }
  if (use_RESET) {
    const int Hl = YCC_HV[YCCtype][0] >> 4;
    const int Vl = YCC_HV[YCCtype][0] & 0xF;
    enc.put_word(DRI);
    enc.put_word(4);
    size_t mcu_x = round_up(width, DCTSIZE * Hl) / (DCTSIZE * Hl);
    size_t mcu_y = BUFLINES / (DCTSIZE * Vl);
    enc.put_word(mcu_x * mcu_y);
  }
  create_SOS(nc, enc);
}