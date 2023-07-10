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

constexpr int32_t DCTSIZE  = 8;
constexpr int32_t DCTSIZE2 = DCTSIZE * DCTSIZE;
constexpr size_t LINES     = 16;
