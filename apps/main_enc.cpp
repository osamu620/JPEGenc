#include <cstdio>
#include <string>
#include <jpegenc.hpp>
#include <chrono>
#include "parse_args.hpp"
#include "pnm.hpp"

int main(int argc, char *argv[]) {
  bool benchmark = false;
  int QF, YCCtype, width, height, nc;
  std::string infile, outfile;
  if (parse_args(argc, argv, infile, outfile, QF, YCCtype, benchmark)) {
    return EXIT_FAILURE;
  }
  uint8_t *imdata = read_pnm(infile, width, height, nc);
  jpegenc::im_info inimg(imdata, width, height, nc);

  size_t duration = 0;
  auto start      = std::chrono::high_resolution_clock::now();
  jpegenc::jpeg_encoder encoder(inimg, QF, YCCtype);
  if (!benchmark) {
    encoder.invoke();
    auto stop = std::chrono::high_resolution_clock::now() - start;
    duration += std::chrono::duration_cast<std::chrono::microseconds>(stop).count();

    printf("Elapsed time for encoding: %7.3lf [ms]\n", static_cast<double>(duration) / 1000.0);
    printf("Throughput: %7.3lf [MP/s]\n", (width * height) / static_cast<double>(duration));
  } else {
    int iter = 0;
    while (1) {
      encoder.invoke();
      iter++;
      auto stop = std::chrono::high_resolution_clock::now() - start;
      duration  = std::chrono::duration_cast<std::chrono::microseconds>(stop).count();
      if ((static_cast<double>(duration) / 1000.0) >= 1000) break;
    }

    double et = 1000.0 / (static_cast<double>(duration) / 1000.0);
    printf("Frames rate: %d [fps]\n", iter);
    printf("Throughput: %7.3lf [MP/s]\n", (width * height * iter * et) / 1000000.0);
  }
  std::free(imdata);

  const std::vector<uint8_t> codestream = encoder.get_codestream();
  std::cout << "Codestream bytes = " << codestream.size() << std::endl;

  FILE *out = fopen(outfile.c_str(), "wb");
  if (out == nullptr) {
    std::cerr << "Could not open '" << outfile << "' as an output file." << std::endl;
    return EXIT_FAILURE;
  }
  fwrite(codestream.data(), sizeof(uint8_t), codestream.size(), out);
  fclose(out);
}