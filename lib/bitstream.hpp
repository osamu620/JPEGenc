#pragma once
#ifndef NDEBUG
  #include <cassert>
#endif

#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>

#include "jpgmarkers.hpp"

#define USE_VECTOR 0

namespace jpegenc_hwy {
[[maybe_unused]] void send_8_bytes(uint8_t *in, uint8_t *out);
}  // namespace jpegenc_hwy

class stream_buf {
 private:
  std::unique_ptr<uint8_t[]> buf;
  size_t len;

 public:
  size_t pos;
  uint8_t *cur_byte;

  stream_buf() : buf(nullptr), len(0), pos(0), cur_byte(nullptr){};
  explicit stream_buf(size_t size) : buf(std::make_unique<uint8_t[]>(size)), len(size) {
    pos      = 0;
    cur_byte = buf.get();
  }

  inline void expand() {
    uint8_t *p                         = buf.release();
    std::unique_ptr<uint8_t[]> new_buf = std::make_unique<uint8_t[]>(len + len);
    memcpy(new_buf.get(), p, len);
    buf = std::move(new_buf);
    len += len;
    delete[] p;
    //    __builtin_prefetch(buf.get() + pos, 0, 1);
    cur_byte = buf.get() + pos;
  }

  inline void put_byte(uint8_t val) {
    if (pos == len) {
      expand();
    }
    *cur_byte++ = val;
    pos++;
  }

  inline void put_qword(uint64_t val) {
    if (pos + 8 > len) {
      expand();
    }
    // emits eight uint8_t values at once
#if (HWY_TARGET | HWY_NEON_WITHOUT_AES) == HWY_NEON_WITHOUT_AES
    *(uint64_t *)cur_byte = __builtin_bswap64(val);
#elif (HWY_TARGET | HWY_NEON) == HWY_NEON
    *(uint64_t *)cur_byte = __builtin_bswap64(val);
#elif HWY_TARGET <= HWY_SSE2
    *(uint64_t *)cur_byte = __bswap_64(val);
#endif
    //    jpegenc_hwy::send_8_bytes((uint8_t *)&val, cur_byte);
    cur_byte += 8;
    pos += 8;
  }

  uint8_t *get_buf() {
    pos      = 0;
    cur_byte = buf.get();
    return buf.get();
  }
};

class bitstream {
 private:
  int32_t bits;
  uint64_t tmp;
#if USE_VECTOR != 0
  std::vector<uint8_t> stream;
#else
  stream_buf stream;
#endif

  inline void emit_qword(uint64_t d) {
    uint64_t val;
    if (d & 0x8080808080808080UL & ~(d + 0x0101010101010101UL)) {
      for (int i = 56; i >= 0; i -= 8) {
        val = d >> i;
        put_byte(val);
        if ((val & 0xFF) == 0xFF) {
          put_byte(0x00);
        }
      }
    } else {
#if USE_VECTOR != 0
      for (int i = 56; i >= 0; i -= 8) {
        val = d >> i;
        put_byte(val);
      }
#else
      stream.put_qword(d);
#endif
    }
  }

  [[maybe_unused]] inline void emit_dword(uint32_t d) {
    uint32_t val;
    if (d & 0x80808080 & ~(d + 0x01010101)) {
      for (int i = 24; i >= 0; i -= 8) {
        val = d >> i;
        put_byte(val);
        if ((val & 0xFF) == 0xFF) {
          put_byte(0x00);
        }
      }
    } else {
      for (int i = 24; i >= 0; i -= 8) {
        val = d >> i;
        put_byte(val);
      }
    }
  }

  void flush() {
    //    int n = (bits + 8 - 1) / 8;
    //    tmp <<= 8 * n - bits;
    //    tmp |= ~(0xFFFFFFFFFFFFFFFFUL << (8 * n - bits));
    const int bits_to_flush = 64 - bits;
    int n                   = (bits_to_flush + 8 - 1) / 8;
    tmp <<= 8 * n - bits_to_flush;
    tmp |= ~(0xFFFFFFFFFFFFFFFFUL << (8 * n - bits_to_flush));
    uint64_t mask = 0xFF00000000000000UL >> (64 - n * 8);
    for (int i = n - 1; i >= 0; --i) {
      uint8_t upper_byte = (tmp & mask) >> (8 * i);
      put_byte(upper_byte);
      if (upper_byte == 0xFF) {
        put_byte(0x00);
      }
      bits -= 8;
      mask >>= 8;
    }
    tmp  = 0;
    bits = 0;
  }

 public:
  //  bitstream() : bits(0), tmp(0) {}

#if USE_VECTOR != 0
  explicit bitstream(size_t length) : bits(0), tmp(0) { stream.reserve(length); }
  inline void put_byte(uint8_t d) { stream.push_back(d); }
#else
  explicit bitstream(size_t length) : bits(64), tmp(0), stream(length) {}
  inline void put_byte(uint8_t d) { stream.put_byte(d); }
#endif

  inline void put_word(uint16_t d) {
    put_byte(d >> 8);
    put_byte(d & 0xFF);
  }

  inline void put_bits(uint32_t cwd, const int32_t len) {
#ifndef NDEBUG
    assert(len > 0);
#endif
    bits -= len;
    if (bits < 0) {
      // PUT_AND_FLUSH
      tmp = (tmp << (len + bits)) | (cwd >> -bits);
      emit_qword(tmp);
      bits += 64;
      tmp = cwd;
    } else {
      tmp = (tmp << len) | cwd;
    }
    // #if not defined(NAIVE)
    //     const int32_t exlen = bits + len;
    //     if (exlen < 64) {
    //       tmp <<= len;
    //       tmp |= (uint64_t)cwd & ((1 << len) - 1);
    //       bits = exlen;
    //     } else {
    //       tmp <<= 64 - bits;
    //       uint64_t mask = (~(0xFFFFFFFFFFFFFFFFU << (exlen - 64)));
    //       uint64_t val  = cwd & mask;
    //       cwd &= ((1 << len) - 1);
    //       cwd >>= (exlen - 64);
    //       tmp |= cwd;
    //       emit_qword(tmp);
    //       tmp  = val;
    //       bits = exlen - 64;
    //     }
    // #else
    //     while (len) {
    //       tmp <<= 1;
    //       tmp |= (cwd >> (len - 1)) & 1;
    //       len--;
    //       bits++;
    //       if (bits == 8) {
    //         put_byte(tmp);
    //         if (tmp == 0xFF) {
    //           // byte stuff
    //           put_byte(0x00);
    //         }
    //         tmp  = 0;
    //         bits = 0;
    //       }
    //     }
    // #endif
  }

  void put_RST(int n) {
    flush();
    put_word(RST[n]);
  }

  [[maybe_unused]] auto get_stream() {
    flush();
    return &stream;
  }

  std::vector<uint8_t> finalize() {
    flush();
    put_word(EOI);
#if USE_VECTOR != 0
    return std::move(stream);
#else
    size_t size = stream.pos;
    std::vector<uint8_t> out;
    out.resize(size);
    memcpy(out.data(), stream.get_buf(), size);
    return out;
#endif
  }
};