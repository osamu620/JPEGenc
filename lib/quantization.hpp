#pragma once

#include <cstdint>
#include <vector>

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

// clang-format off
constexpr float qmatrix[2][64] = {
    {16, 11, 10, 16,  24,  40,  51, 61,
     12, 12, 14, 19,  26,  58,  60, 55,
     14, 13, 16, 24,  40,  57,  69, 56,
     14, 17, 22, 29,  51,  87,  80, 62,
     18, 22, 37, 56,  68, 109, 103, 77,
     24, 35, 55, 64,  81, 104, 113, 92,
     49, 64, 78, 87, 103, 121, 120, 101, 
     72, 92, 95, 98, 112, 100, 103, 99},
    {17, 18, 24, 47, 99, 99, 99, 99,
     18, 21, 26, 66, 99, 99, 99, 99,
     24, 26, 56, 99, 99, 99, 99, 99,
     47, 66, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99}};
// clang-format on

void create_scaled_qtable(int c, int QF, int16_t *qtable);

HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
void quantize_core(int16_t *HWY_RESTRICT data, const int16_t *HWY_RESTRICT qtable);
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy
HWY_AFTER_NAMESPACE();