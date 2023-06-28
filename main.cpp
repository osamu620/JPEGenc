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
  imchunk iii(infile);

  const int width = iii.get_width();
  const int height = iii.get_height();
  const int nc = iii.get_num_comps();

  if (nc == 1) {
    YCCtype = YCC::GRAY;
  }

  int qtable_L[64], qtable_C[64];
  create_qtable(0, QF, qtable_L);
  create_qtable(1, QF, qtable_C);
  int fqtable_L[64], fqtable_C[64];
  create_qtable2(0, QF, fqtable_L);
  create_qtable2(1, QF, fqtable_C);
  bitstream enc;
  create_mainheader(width, height, nc, qtable_L, qtable_C, YCCtype, enc);

  std::vector<int16_t *> YUV(nc);
  int bufsize = width * fx * nc * 32 * fy;
  YUV[0] = new int16_t[width * nc * 32];
  for (int c = 0; c < nc; ++c) {
    YUV[c] = new int16_t[bufsize];
  }
  std::vector<int16_t *> zbuf(nc);
  zbuf[0] = new int16_t[width * nc * 32];
  for (int c = 0; c < nc; ++c) {
    zbuf[c] = new int16_t[bufsize];
  }
  std::vector<int> prev_dc(3, 0);
  auto s0 = std::chrono::high_resolution_clock::now();
  for (int n = height; n > 0; n -= LINES) {
    int16_t *src = iii.get_lines();
    if (nc == 3) {
      rgb2ycbcr(width, src);
    }
    subsample(src, YUV, width, 32, fx, fy);
    blkdct2(YUV, width, fx, fy);
    blkquantize(YUV, fqtable_L, fqtable_C, width, fx, fy);

    make_zigzag_buffer(YUV, zbuf, width, YCCtype);
    encode_MCUs(zbuf, width, YCCtype, prev_dc, enc);
    iii.advance();
  }
  auto d0 = std::chrono::high_resolution_clock::now() - s0;
  auto c0 = std::chrono::duration_cast<std::chrono::microseconds>(d0).count();
  printf("Elapsed time for encoding : %7.3lf [ms]\n", static_cast<double>(c0) / 1000.0);
  // Finalize codestream
  const std::vector<uint8_t> codestream = enc.finalize();
  std::cout << "Codestream bytes = " << codestream.size() << std::endl;

  fwrite(codestream.data(), sizeof(uint8_t), codestream.size(), out);
  fclose(out);
  for (int c = 0; c < nc; ++c) {
    delete[] YUV[c];
    delete[] zbuf[c];
  }

  return EXIT_SUCCESS;
}