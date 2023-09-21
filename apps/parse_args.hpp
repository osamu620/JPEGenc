#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

enum YCC { YUV444, YUV422, YUV411, YUV440, YUV420, YUV410, GRAY, GRAY2 };

void print_help() {
  std::cout << "Usage: ./jpenc -i input.ppm -o output.jpg [-q quality] [-c color-subsampling-type]"
            << std::endl;
  std::cout << "  -i input .ppm file" << std::endl;
  std::cout << "  -o output .jpg file" << std::endl;
  std::cout << "  -q quality 0-100, default = 75" << std::endl;
  std::cout << "  -c color subsampling type, default = 420" << std::endl;
  std::cout << "   (444, 422, 420, 440, 411, 410, GRAY)" << std::endl;
  std::cout << "  -h print this help" << std::endl;
}

int parse_args(int argc, char **&argv, std::string &inname, std::string &outname, int &QF, int &YCCtype,
               bool &benchmark) {
  YCCtype = YCC::YUV420;
  QF      = 75;
  std::vector<std::string> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  if (args.size() == 1) {
    print_help();
    return EXIT_FAILURE;
  }
  bool ifile_exist = false, ofile_exist = false;
  for (size_t i = 1; i < args.size(); ++i) {
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
          ofile_exist = true;
          outname     = args[i + 1];
          ++i;
        } else if (args[i].substr(1) == "q") {
          try {
            QF = std::stoi(args[i + 1], nullptr, 10);
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
          } else if (args[i + 1] == "GRAY") {
            YCCtype = YCC::GRAY2;
          } else {
            std::cerr << "Unknown chroma format " << args[i + 1] << std::endl;
            return EXIT_FAILURE;
          }
          ++i;
        } else if (args[i].substr(1) == "b") {
          benchmark = true;
        } else if (args[i].substr(1) == "h") {
          print_help();
          return EXIT_FAILURE;
        } else {
          std::cerr << "Unknown option " << args[i].substr(1) << std::endl;
          return EXIT_FAILURE;
        }
        break;
      default:
        std::cerr << "Unknown option " << args[i] << std::endl;
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