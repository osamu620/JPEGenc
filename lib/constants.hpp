#pragma once
#include <cstdint>
#include <cstddef>

// Attributes
#if __GNUC__ || __has_attribute(always_inline)
  #define FORCE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
  #define FORCE_INLINE __forceinline
#else
  #define FORCE_INLINE inline
#endif

// Constants
constexpr int32_t DCTSIZE  = 8;
constexpr int32_t DCTSIZE2 = DCTSIZE * DCTSIZE;
constexpr int32_t LINES    = 16;

// Macros
#define round_up(x, n) (((x) + (n)-1) & (-n))
#define Padd(d, V2, V1) ConcatEven((d), Add(DupEven((V1)), DupOdd((V1))), Add(DupEven((V2)), DupOdd((V2))))
#if defined(_MSC_VER) && defined(_WIN64)
  #define JPEGENC_CLZ32(x) _lzcnt_u32((x))
  #define JPEGENC_CLZ64(x) _lzcnt_u64((x))
#else
  #define JPEGENC_CLZ32(x) __builtin_clz((x))
  #define JPEGENC_CLZ64(x) __builtin_clzll((x))
#endif