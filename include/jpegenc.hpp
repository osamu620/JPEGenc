#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace jpegenc {

struct im_info {
  uint8_t *data;
  const int32_t width;
  const int32_t height;
  const int32_t nc;
  im_info(uint8_t *buf, int32_t w, int32_t h, int32_t c) : data(buf), width(w), height(h), nc(c) {}
};

class jpeg_encoder {
 private:
  std::unique_ptr<class jpeg_encoder_impl> impl;
  std::vector<uint8_t> codestream;

 public:
  jpeg_encoder(im_info &inimg, int &QF, int &YCCtype);
  void invoke();
  std::vector<uint8_t> get_codestream();
  ~jpeg_encoder();
};
}  // namespace jpegenc