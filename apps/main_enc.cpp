#include <cstdio>
#include "parse_args.hpp"
#include "../include/jpegenc.hpp"
#include <string>
#include <chrono>

int main(int argc, char *argv[]) {
  int QF, YCCtype;
  std::string infile;
  FILE *out = nullptr;
  if (parse_args(argc, argv, infile, &out, QF, YCCtype)) {
    return EXIT_FAILURE;
  }
  int width, height;
  jpegenc::jpeg_encoder encoder(infile, QF, YCCtype, width, height);
  size_t duration = 0;

  auto start = std::chrono::high_resolution_clock::now();
  encoder.invoke();
  auto stop = std::chrono::high_resolution_clock::now() - start;
  duration += std::chrono::duration_cast<std::chrono::microseconds>(stop).count();

  printf("Elapsed time for encoding: %7.3lf [ms]\n", static_cast<double>(duration) / 1000.0);
  printf("Throughput: %7.3lf [MP/s]\n", (width * height) / static_cast<double>(duration));

  const std::vector<uint8_t> codestream = encoder.get_codestream();
  std::cout << "Codestream bytes = " << codestream.size() << std::endl;

  fwrite(codestream.data(), sizeof(uint8_t), codestream.size(), out);
  fclose(out);
}