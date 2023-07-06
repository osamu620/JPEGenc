#pragma once
#include <cstdint>
#include <cstddef>

#if __GNUC__ || __has_attribute(always_inline)
  #define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#else
  #define FORCE_INLINE inline
#endif

#define JPEG_USE_NEON

constexpr int32_t DCTSIZE = 8;
constexpr size_t LINES    = 16;

#if defined(JPEG_USE_NEON)
constexpr int32_t FRACBITS = 8;  // shall be 8 with NEON version of DCT
#else
constexpr int32_t FRACBITS = 13;  // shall be less than 13 with non-SIMD DCT
#endif
