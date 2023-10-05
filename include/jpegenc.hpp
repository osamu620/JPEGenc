#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <stdexcept>

#if defined(_MSC_VER)
  #define JPEGENC_EXPORT __declspec(dllexport)
#else
  #define JPEGENC_EXPORT
#endif

#define round_up(x, n) (((x) + (n)-1) & (-n))

namespace jpegenc {

struct im_info {
  FILE *data;
  const size_t pos;
  const int32_t width;
  const int32_t height;
  const int32_t nc;
  im_info(FILE *buf, size_t fpos, int32_t w, int32_t h, int32_t c)
      : data(buf), pos(fpos), width(w), height(h), nc(c) {}
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