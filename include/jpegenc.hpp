#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace jpegenc {
class jpeg_encoder {
 private:
  std::unique_ptr<class jpeg_encoder_impl> impl;

 public:
  std::vector<uint8_t> codestream;
  jpeg_encoder(const std::string &infile, int &QF, int &YCCtype);
  void invoke();
  int32_t get_width();
  int32_t get_height();
  std::vector<uint8_t> get_codestream();
  ~jpeg_encoder();
};
}  // namespace jpegenc