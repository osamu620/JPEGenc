#include <chrono>
#include <cstdio>
#include <iostream>
#include <vector>

#include "block_coding.hpp"
#include "color.hpp"
#include "dct.hpp"
#include "image_chunk.hpp"
#include "jpgheaders.hpp"
#include "parse_args.hpp"
#include "quantization.hpp"
#include "ycctype.hpp"
int main(int argc, char *argv[]) {
  double fx, fy;
  int QF, YCCtype;
  std::string infile;
  FILE *out = nullptr;
  if (parse_args(argc, argv, infile, &out, QF, YCCtype, fx, fy)) {
    return EXIT_FAILURE;
  }
  imchunk image(infile);

  const int width  = image.get_width();
  const int height = image.get_height();
  const int nc     = image.get_num_comps();

  if (nc == 1) {
    YCCtype = YCC::GRAY;
  }

  int qtable_L[64], qtable_C[64];
  create_qtable(0, QF, qtable_L);
  create_qtable(1, QF, qtable_C);

  bitstream enc;
  create_mainheader(width, height, nc, QF, YCCtype, enc);

  std::vector<int16_t *> yuv(nc);
  int bufsize = width * fx * 32 * fy;
  yuv[0]      = new int16_t[width * 32];
  for (int c = 1; c < nc; ++c) {
    yuv[c] = new int16_t[bufsize];
  }
  std::vector<int16_t *> zbuf(nc);
  zbuf[0] = new int16_t[width * 32];
  for (int c = 1; c < nc; ++c) {
    zbuf[c] = new int16_t[bufsize];
  }
  std::vector<int> prev_dc(3, 0);

  size_t c0 = 0;
  auto s0   = std::chrono::high_resolution_clock::now();
  for (int n = height; n > 0; n -= LINES) {
    int16_t *src = image.get_lines();
    if (nc == 3) {
      rgb2ycbcr(width, src);
    }

    subsample(src, yuv, width, YCCtype);
    dct2(yuv, width, fx, fy);
    quantize(yuv, qtable_L, qtable_C, width, YCCtype);
    make_zigzag_buffer(yuv, zbuf, width, YCCtype);

    encode_MCUs(zbuf, width, YCCtype, prev_dc, enc);

    image.advance();
  }
  auto d0 = std::chrono::high_resolution_clock::now() - s0;
  c0 += std::chrono::duration_cast<std::chrono::microseconds>(d0).count();
  printf("Elapsed time for encoding : %7.3lf [ms]\n", static_cast<double>(c0) / 1000.0);
  printf("Throughput : %7.3lf [MP/s]\n", (width * height * nc) / static_cast<double>(c0));
  // Finalize codestream
  const std::vector<uint8_t> codestream = enc.finalize();
  std::cout << "Codestream bytes = " << codestream.size() << std::endl;

  fwrite(codestream.data(), sizeof(uint8_t), codestream.size(), out);
  fclose(out);
  for (int c = 0; c < nc; ++c) {
    delete[] yuv[c];
    delete[] zbuf[c];
  }

  return EXIT_SUCCESS;
}