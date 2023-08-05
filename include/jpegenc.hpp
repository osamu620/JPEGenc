#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#if defined(_MSC_VER)
  #define JPEGENC_EXPORT __declspec(dllexport)
#else
  #define JPEGENC_EXPORT
#endif

namespace jpegenc {

struct im_info {
  uint8_t *data;
  const int32_t width;
  const int32_t height;
  const int32_t nc;
  im_info(uint8_t *buf, int32_t w, int32_t h, int32_t c) : data(buf), width(w), height(h), nc(c) {
    if ((width % 16) || (height % 16)) {
      std::free(data);
      throw std::runtime_error("ERROR: image size error\n");
    }
  }
};

class jpeg_encoder {
 private:
  std::unique_ptr<class jpeg_encoder_impl> impl;
  std::vector<uint8_t> codestream;

 public:
  JPEGENC_EXPORT jpeg_encoder(im_info &inimg, int &QF, int &YCCtype);
  JPEGENC_EXPORT void invoke();
  JPEGENC_EXPORT std::vector<uint8_t> get_codestream();
  JPEGENC_EXPORT ~jpeg_encoder();
};
}  // namespace jpegenc