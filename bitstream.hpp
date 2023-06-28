#pragma once
#include <cassert>
#include <cstdint>
#include <vector>

#include "jpgmarkers.hpp"

class bitstream {
 private:
  int32_t bits;
  uint8_t tmp;
  std::vector<uint8_t> stream;

 public:
  bitstream() : bits(0), tmp(0) { put_dword(SOI); }

  void put_byte(uint8_t d) { stream.push_back(d); }

  void put_dword(uint16_t d) {
    put_byte(d >> 8);
    put_byte(d & 0xFF);
  }

  void put_bits(uint32_t cwd, uint32_t len) {
    assert(len != 0);
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
  }

  void flush() {
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
  }

  auto finalize() {
    flush();
    put_dword(EOI);
    return std::move(stream);
  }
};