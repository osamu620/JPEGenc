#pragma once
#include <cstdint>

#define JPEG_USE_NEON

constexpr int32_t DCTSIZE = 8;
constexpr size_t LINES    = 16;

#if defined(JPEG_USE_NEON)
constexpr int32_t FRACBITS = 8;  // shall be 8 with NEON version of DCT
#else
constexpr int32_t FRACBITS = 8;  // shall be less than 13 with non-SIMD DCT
#endif
