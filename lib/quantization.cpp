#include <cmath>
#include "ycctype.hpp"
#include "constants.hpp"
#include "quantization.hpp"

#if defined(JPEG_USE_NEON)
  #include <arm_neon.h>
#endif
#if defined(JPEG_USE_AVX2)
  #include <x86intrin.h>
#endif
// clang-format off
constexpr float qscale[64] = {
        0.125000000000000, 0.090119977750868, 0.095670858091272, 0.106303761845907, 0.125000000000000, 0.159094822571604, 0.230969883127822, 0.453063723176444
        , 0.090119977750868, 0.064972883118536, 0.068974844820736, 0.076640741219094, 0.090119977750868, 0.114700974963451, 0.166520005828800, 0.326640741219094
        , 0.095670858091272, 0.068974844820736, 0.073223304703363, 0.081361376913026, 0.095670858091272, 0.121765905546433, 0.176776695296637, 0.346759961330537
        , 0.106303761845907, 0.076640741219094, 0.081361376913026, 0.090403918260731, 0.106303761845907, 0.135299025036549, 0.196423739596776, 0.385299025036549
        , 0.125000000000000, 0.090119977750868, 0.095670858091272, 0.106303761845907, 0.125000000000000, 0.159094822571604, 0.230969883127822, 0.453063723176444
        , 0.159094822571604, 0.114700974963451, 0.121765905546433, 0.135299025036549, 0.159094822571604, 0.202489300552722, 0.293968900604840, 0.576640741219094
        , 0.230969883127822, 0.166520005828800, 0.176776695296637, 0.196423739596776, 0.230969883127822, 0.293968900604840, 0.426776695296637, 0.837152601532152
        , 0.453063723176444, 0.326640741219094, 0.346759961330537, 0.385299025036549, 0.453063723176444, 0.576640741219094, 0.837152601532152, 1.642133898068012
};
// clang-format on

void create_qtable(int c, int QF, int *qtable) {
  float scale = (QF < 50) ? 5000.0F / static_cast<float>(QF) : 200.0F - 2.0F * static_cast<float>(QF);
  for (int i = 0; i < 64; ++i) {
    float stepsize = (qmatrix[c][i] * scale + 50.0F) / 100.0F;
    int val;
    stepsize = floor(stepsize);
    if (stepsize < 1.0F) {
      stepsize = 1.0F;
    }
    if (stepsize > 255.0F) {
      stepsize = 255.0F;
    }
    // val = static_cast<int>(lround((qscale[i] / stepsize) * (1 << 16)));
    val = static_cast<int>((qscale[i] / stepsize) * (1 << 16) + 0.5);

    //     val       = (val > 0x7FFFU) ? 0x7FFF : val;
    qtable[i] = val;
  }
}

static inline void quantize_fwd(int16_t *in, const int *qtable) {
#if not defined(JPEG_USE_NEON) && not defined(JPEG_USE_AVX2)
  int shift = 16;
  int half  = 1 << (shift - 1);
  for (int i = 0; i < DCTSIZE2; ++i) {
    in[i] = static_cast<int16_t>((in[i] * qtable[i] + half) >> shift);
  }
#elif defined(JPEG_USE_AVX2)
  //  int shift = 16;
  //  int half  = 1 << (shift - 1);
  //  for (int i = 0; i < DCTSIZE2; ++i) {
  //    in[i] = static_cast<int16_t>((in[i] * qtable[i] + half) >> shift);
  //  }
  __m256i half = _mm256_set1_epi32(1 << 15);
  for (int i = 0; i < DCTSIZE2; i += DCTSIZE * 2) {
    __m256i ql = _mm256_load_si256((__m256i *)qtable);
    __m256i qh = _mm256_load_si256((__m256i *)(qtable + 8));
    __m256i v  = _mm256_load_si256((__m256i *)in);
    __m256i vl = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 0));
    __m256i vh = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 1));
    vl         = _mm256_mullo_epi32(vl, ql);
    vh         = _mm256_mullo_epi32(vh, qh);
    __m256i sl = vl;
    __m256i sh = vh;
    vl         = _mm256_abs_epi32(vl);
    vh         = _mm256_abs_epi32(vh);
    vl         = _mm256_srai_epi32(_mm256_add_epi32(vl, half), 16);
    vh         = _mm256_srai_epi32(_mm256_add_epi32(vh, half), 16);
    vl         = _mm256_sign_epi32(vl, sl);
    vh         = _mm256_sign_epi32(vh, sh);
    _mm256_store_si256((__m256i *)in, _mm256_permute4x64_epi64(_mm256_packs_epi32(vl, vh), 0xD8));
    in += DCTSIZE * 2;
    qtable += DCTSIZE * 2;
  }
#elif defined(JPEG_USE_NEON)
  for (int i = 0; i < DCTSIZE2; i += DCTSIZE) {
    int32x4_t ql = vld1q_s32(qtable);
    int32x4_t qh = vld1q_s32(qtable + 4);
    int16x8_t v  = vld1q_s16(in);
    int32x4_t vl = vmovl_s16(vget_low_s16(v));
    int32x4_t vh = vmovl_s16(vget_high_s16(v));
    vl           = vmulq_s32(vl, ql);
    vh           = vmulq_s32(vh, qh);
    vl           = vrshrq_n_s32(vl, 16);
    vh           = vrshrq_n_s32(vh, 16);
    vst1q_s16(in, vcombine_s16(vmovn_s32(vl), vmovn_s32(vh)));
    in += DCTSIZE;
    qtable += DCTSIZE;
  }
#endif
}

void quantize(std::vector<int16_t *> in, int *qtableL, int *qtableC, int width, int YCCtype) {
  int scale_x = YCC_HV[YCCtype][0] >> 4;
  int scale_y = YCC_HV[YCCtype][0] & 0xF;
  int nc      = (YCCtype == YCC::GRAY || YCCtype == YCC::GRAY2) ? 1 : 3;

  for (int i = 0; i < width * LINES; i += DCTSIZE2) {
    quantize_fwd(in[0] + i, qtableL);
  }
  for (int c = 1; c < nc; ++c) {
    for (int i = 0; i < width / scale_x * LINES / scale_y; i += DCTSIZE2) {
      quantize_fwd(in[c] + i, qtableC);
    }
  }
}