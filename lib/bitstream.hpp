#pragma once
#ifndef NDEBUG
  #include <cassert>
#endif

#include <cstdint>
#include <vector>

#include "jpgmarkers.hpp"

// #define NAIVE

class bitstream {
 private:
  int32_t bits;
  uint64_t tmp;
  std::vector<uint8_t> stream;

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
      for (int i = 56; i >= 0; i -= 8) {
        val = d >> i;
        put_byte(val);
      }
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
#if not defined(NAIVE)
    int n = (bits + 8 - 1) / 8;
    tmp <<= 8 * n - bits;
    tmp |= ~(0xFFFFFFFFFFFFFFFFUL << (8 * n - bits));
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
#else
    if (bits) {
      // stuff bit = '1'
      uint8_t stuff = 0xFFU >> bits;
      tmp <<= (8 - bits);
      tmp |= stuff;
      put_byte(tmp);
      if (tmp == 0xFF) {
        // byte stuff
        put_byte(0x00);
      }
    }
    tmp  = 0;
    bits = 0;
#endif
  }

 public:
  bitstream() : bits(0), tmp(0) {}
  explicit bitstream(size_t length) : bits(0), tmp(0) { stream.reserve(length); }

  inline void put_byte(uint8_t d) { stream.push_back(d); }

  inline void put_word(uint16_t d) {
    put_byte(d >> 8);
    put_byte(d & 0xFF);
  }

  inline void put_bits(uint32_t cwd, const int32_t len) {
#ifndef NDEBUG
    assert(len > 0);
#endif
#if not defined(NAIVE)
    const int32_t exlen = bits + len;
    if (exlen < 64) {
      tmp <<= len;
      tmp |= (uint64_t)cwd & ((1 << len) - 1);
      bits = exlen;
    } else {
      tmp <<= 64 - bits;
      uint64_t mask = (~(0xFFFFFFFFFFFFFFFFU << (exlen - 64)));
      uint64_t val  = cwd & mask;
      cwd &= ((1 << len) - 1);
      cwd >>= (exlen - 64);
      tmp |= cwd;
      emit_qword(tmp);
      tmp  = val;
      bits = exlen - 64;
    }
#else
    while (len) {
      tmp <<= 1;
      tmp |= (cwd >> (len - 1)) & 1;
      len--;
      bits++;
      if (bits == 8) {
        put_byte(tmp);
        if (tmp == 0xFF) {
          // byte stuff
          put_byte(0x00);
        }
        tmp  = 0;
        bits = 0;
      }
    }
#endif
  }

  void put_RST(int n) {
    flush();
    put_word(RST[n]);
  }
  auto get_stream() {
    flush();
    return &stream;
  }

  std::vector<uint8_t> finalize() {
    flush();
    put_word(EOI);
    return std::move(stream);
  }
};