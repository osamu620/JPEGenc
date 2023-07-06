#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>
#include <memory>

#include "block_coding.hpp"
#include "color.hpp"
#include "dct.hpp"
#include "image_chunk.hpp"
#include "jpgheaders.hpp"
#include "parse_args.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"

void construct_line_buffer(std::vector<std::unique_ptr<int16_t[]>> &in, size_t size0, size_t size1) {
  in[0] = std::make_unique<int16_t[]>(size0 * LINES);
  for (int c = 1; c < in.size(); ++c) {
    in[c] = std::make_unique<int16_t[]>(size1);
  }
}

int main(int argc, char *argv[]) {
  int QF, YCCtype;
  std::string infile;
  FILE *out = nullptr;
  if (parse_args(argc, argv, infile, &out, QF, YCCtype)) {
    return EXIT_FAILURE;
  }
  const int scale_x = YCC_HV[YCCtype][0] >> 4;
  const int scale_y = YCC_HV[YCCtype][0] & 0xF;

  imchunk image(infile);
  const int width  = image.get_width();
  const int height = image.get_height();
  const int nc     = image.get_num_comps();

  if (nc == 1) {
    YCCtype = YCC::GRAY;
  }

  const size_t bufsize_L = width * LINES;
  const size_t bufsize_C = width / scale_x * LINES / scale_y;

  // Prepare line-buffers
  std::vector<std::unique_ptr<int16_t[]>> line_buffer(nc);
  std::vector<std::unique_ptr<int16_t[]>> line_buffer_zigzag(nc);
  construct_line_buffer(line_buffer, bufsize_L, bufsize_C);
  construct_line_buffer(line_buffer_zigzag, bufsize_L, bufsize_C);

  std::vector<int16_t *> yuv(nc);
  yuv[0] = line_buffer[0].get();
  for (int c = 1; c < nc; ++c) {
    yuv[c] = line_buffer[c].get();
  }
  std::vector<int16_t *> mcu(nc);
  mcu[0] = line_buffer_zigzag[0].get();
  for (int c = 1; c < nc; ++c) {
    mcu[c] = line_buffer_zigzag[c].get();
  }

  std::vector<int> prev_dc(3, 0);

  int16_t qtable_L[64], qtable_C[64];
  create_qtable(0, QF, qtable_L);
  create_qtable(1, QF, qtable_C);

  bitstream enc;
  create_mainheader(width, height, nc, QF, YCCtype, enc);
  size_t c0 = 0;
  auto s0   = std::chrono::high_resolution_clock::now();

  // Encoding
  for (int n = 0; n < height / LINES; ++n) {
    int16_t *src = image.get_lines(n);
    if (nc == 3) {
      rgb2ycbcr(src, width);
    }
    subsample(src, yuv, width, YCCtype);
    dct2(yuv, width, YCCtype);
    quantize(yuv, qtable_L, qtable_C, width, YCCtype);
    construct_MCUs(yuv, mcu, width, YCCtype);
    encode_MCUs(mcu, width, YCCtype, prev_dc, enc);
  }
  auto d0 = std::chrono::high_resolution_clock::now() - s0;
  c0 += std::chrono::duration_cast<std::chrono::microseconds>(d0).count();
  printf("Elapsed time for encoding : %7.3lf [ms]\n", static_cast<double>(c0) / 1000.0);
  printf("Throughput : %7.3lf [MP/s]\n", (width * height) / static_cast<double>(c0));

  // Finalize codestream
  const std::vector<uint8_t> codestream = enc.finalize();
  std::cout << "Codestream bytes = " << codestream.size() << std::endl;

  fwrite(codestream.data(), sizeof(uint8_t), codestream.size(), out);
  fclose(out);

  return EXIT_SUCCESS;
}