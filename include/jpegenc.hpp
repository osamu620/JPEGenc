#pragma once
#include <chrono>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <iostream>
#include <vector>

namespace jpegenc {
class jpeg_encoder {
 private:
  std::unique_ptr<class jpeg_encoder_impl> impl;

 public:
  std::vector<uint8_t> codestream;
  jpeg_encoder(const std::string &infile, int &QF, int &YCCtype, int &width, int &height);
  void invoke();
  std::vector<uint8_t> get_codestream();
  ~jpeg_encoder();
};
}  // namespace jpegenc