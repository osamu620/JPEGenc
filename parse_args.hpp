#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "ycctype.hpp"

int parse_args(int argc, char *argv[], std::string &inname, FILE **out, int &QF, int &YCCtype) {
  YCCtype = YCC::YUV420;
  QF      = 75;
  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  if (args.size() == 1) {
    std::cout << "print help" << std::endl;
    return EXIT_FAILURE;
  }
  bool ifile_exist = false, ofile_exist = false;
  for (int i = 1; i < args.size(); ++i) {
    switch (args[i][0]) {
      case '-':
        if (args[i].substr(1) == "i") {
          if (!std::filesystem::is_regular_file(args[i + 1])) {
            std::cerr << "Could not open " << args[i + 1] << " as an input file." << std::endl;
            return EXIT_FAILURE;
          }
          ifile_exist = true;
          inname      = args[i + 1];
          ++i;
        } else if (args[i].substr(1) == "o") {
          *out = fopen(args[i + 1].c_str(), "wb");
          if (*out == nullptr) {
            std::cerr << "Could not open '" << args[i + 1] << "' as an output file." << std::endl;
            return EXIT_FAILURE;
          }
          ofile_exist = true;
          ++i;
        } else if (args[i].substr(1) == "q") {
          try {
            QF = std::stol(args[i + 1], nullptr, 10);
          } catch (std::invalid_argument &e) {
            std::cerr << "QF value is missing." << std::endl;
            return EXIT_FAILURE;
          }

          if (QF < 0 || QF > 100) {
            std::cerr << "Qfactor value shall be in the range from 0 to 100." << std::endl;
            return EXIT_FAILURE;
          }
          QF = (QF == 0) ? 1 : QF;
          ++i;
        } else if (args[i].substr(1) == "c") {
          if (args[i + 1] == "444") {
            YCCtype = YCC::YUV444;
          } else if (args[i + 1] == "422") {
            YCCtype = YCC::YUV422;
          } else if (args[i + 1] == "411") {
            YCCtype = YCC::YUV411;
          } else if (args[i + 1] == "440") {
            YCCtype = YCC::YUV440;
          } else if (args[i + 1] == "410") {
            YCCtype = YCC::YUV410;
          } else if (args[i + 1] == "420") {
            YCCtype = YCC::YUV420;
          } else {
            std::cerr << "Unknown chroma format " << args[i + 1] << std::endl;
            return EXIT_FAILURE;
          }
          ++i;
        } else {
          std::cerr << "Unkown option " << args[i].substr(1) << std::endl;
          return EXIT_FAILURE;
        }
        break;
      default:
        std::cerr << "Unkown option " << args[i] << std::endl;
        return EXIT_FAILURE;
        break;
    }
  }
  if (!ifile_exist) {
    std::cerr << "Input file is missing." << std::endl;
    return EXIT_FAILURE;
  }
  if (!ofile_exist) {
    std::cerr << "Output file is missing." << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}