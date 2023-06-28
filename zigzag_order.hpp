#pragma once

#include <cstddef>
// clang-format off
constexpr size_t scan[64] = {
    0,  1,  8,  16, 9,  2,  3,  10,
    17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63
};

constexpr size_t z_stride[64] = {
    0, 0, 1, 2, 1, 0, 0, 1, 
    2, 3, 4, 3, 2, 1, 0, 0, 
    1, 2, 3, 4, 5, 6, 5, 4, 
    3, 2, 1, 0, 0, 1, 2, 3, 
    4, 5, 6, 7, 7, 6, 5, 4, 
    3, 2, 1, 2, 3, 4, 5, 6, 
    7, 7, 6, 5, 4, 3, 4, 5, 
    6, 7, 7, 6, 5, 6, 7, 7
};

constexpr size_t z_plus[64] = {
    0, 1, 0, 0, 1, 2, 3, 2, 
    1, 0, 0, 1, 2, 3, 4, 5, 
    4, 3, 2, 1, 0, 0, 1, 2, 
    3, 4, 5, 6, 7, 6, 5, 4, 
    3, 2, 1, 0, 1, 2, 3, 4, 
    5, 6, 7, 7, 6, 5, 4, 3, 
    2, 3, 4, 5, 6, 7, 7, 6, 
    5, 4, 5, 6, 7, 7, 6, 7
};
// clang-format on