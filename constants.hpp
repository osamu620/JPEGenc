#pragma once
#include <cstdint>

constexpr int32_t DCTSIZE  = 8;
constexpr size_t LINES     = 16;
constexpr int32_t FRACBITS = 13;  // 13: 16 - 1(sign) - 1(guard) - 1(integer)

#define JPEG_USE_NEON