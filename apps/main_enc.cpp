#include <cstdio>
#include <string>
#include <jpegenc.hpp>
#include <chrono>
#include "parse_args.hpp"

int main(int argc, char *argv[]) {
  int QF, YCCtype;
  std::string infile, outfile;
  if (parse_args(argc, argv, infile, outfile, QF, YCCtype)) {
    return EXIT_FAILURE;
  }
  jpegenc::jpeg_encoder encoder(infile, QF, YCCtype);
  const int width  = encoder.get_width();
  const int height = encoder.get_height();
  size_t duration  = 0;

  auto start = std::chrono::high_resolution_clock::now();
  encoder.invoke();
  auto stop = std::chrono::high_resolution_clock::now() - start;
  duration += std::chrono::duration_cast<std::chrono::microseconds>(stop).count();

  printf("Elapsed time for encoding: %7.3lf [ms]\n", static_cast<double>(duration) / 1000.0);
  printf("Throughput: %7.3lf [MP/s]\n", (width * height) / static_cast<double>(duration));

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