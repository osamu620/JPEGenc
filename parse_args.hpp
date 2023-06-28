#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "ycctype.hpp"

int parse_args(int argc, char *argv[], std::string &inname, FILE **out, int &QF, int &YCCtype, double &fx,
               double &fy) {
  YCCtype = YCC::YUV420;
  fx = 0.5;
  fy = 0.5;
  QF = 75;
  std::vector<std::string> args;
  for (int i = 0; i < argc; ++i) {
    args.push_back(argv[i]);
  }

  if (args.size() == 1) {
    std::cout << "print help" << std::endl;
    return EXIT_FAILURE;
  }
  bool ifile_exist = false, ofile_exist = false;
  for (int i = 1; i < args.size(); ++i) {
    switch (args[i][0]) {
      case '-':
        if (args[i].substr(1).compare("i") == 0) {
          if (std::filesystem::is_regular_file(args[i + 1]) == false) {
            std::cerr << "Could not open " << args[i + 1] << " as an input file." << std::endl;
            return EXIT_FAILURE;
          }
          ifile_exist = true;
          inname = args[i + 1];
          ++i;
        } else if (args[i].substr(1).compare("o") == 0) {
          *out = fopen(args[i + 1].c_str(), "wb");
          if (*out == nullptr) {
            std::cerr << "Could not open '" << args[i + 1] << "' as an output file." << std::endl;
            return EXIT_FAILURE;
          }
          ofile_exist = true;
          ++i;
        } else if (args[i].substr(1).compare("q") == 0) {
          try {
            QF = std::stol(args[i + 1], nullptr, 10);
          } catch (std::invalid_argument e) {
            std::cerr << "QF value is missing." << std::endl;
            return EXIT_FAILURE;
          }

          if (QF < 0 || QF > 100) {
            std::cerr << "Qfactor value shall be in the range from 0 to 100." << std::endl;
            return EXIT_FAILURE;
          }
          QF = (QF == 0) ? 1 : QF;
          ++i;
        } else if (args[i].substr(1).compare("c") == 0) {
          if (args[i + 1].compare("444") == 0) {
            YCCtype = YCC::YUV444;
            fx = fy = 1.0;
          } else if (args[i + 1].compare("422") == 0) {
            YCCtype = YCC::YUV422;
            fx = 0.5;
            fy = 1.0;
          } else if (args[i + 1].compare("411") == 0) {
            YCCtype = YCC::YUV411;
            fx = 0.25;
            fy = 1.0;
          } else if (args[i + 1].compare("440") == 0) {
            YCCtype = YCC::YUV440;
            fx = 1.0;
            fy = 0.5;
          } else if (args[i + 1].compare("410") == 0) {
            YCCtype = YCC::YUV410;
            fx = 0.25;
            fy = 0.5;
          } else if (args[i + 1].compare("420") == 0) {
            YCCtype = YCC::YUV420;
            fx = 0.5;
            fy = 0.5;
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