#pragma once
#include <stdio.h>
#include <stdlib.h>

#include <string>

#define DONE 3

/**
 * @fn
 * read pgm file
 * @brief read pgm file, both ASCII and binary formats are supported
 * @param (name) file name of input .pgm
 * @param (width) address of a variable to store the width of the input
 * @param (height) address of a variable to store theheight of the input
 * @param (maxval) address of a variable to store themax pixel value of the
 * input
 * @return returns an address of buffer allocated inside this function. free()
 * shall be called in the user code.
 * @author Osamu Watanabe
 */
unsigned char *read_pnm(const std::string &name, int &width, int &height, int &ncomp) {
  FILE *fp = fopen(name.c_str(), "rb");
  if (fp == nullptr) {
    printf("File %s is not found.\n", name.c_str());
    exit(EXIT_FAILURE);
  }
  int maxval;
  int status = 0;  // status, = 3 is DONE
  int c;
  int val = 0;
  char comment[256];  // temporal buffer to eat comments
  c = fgetc(fp);
  if (c != 'P') {
    printf("This image is not in PNM format.\n");
    exit(EXIT_FAILURE);
  }
  c            = fgetc(fp);
  bool isASCII = false;
  if (c != '5' && c != '2' && c != '6' && c != '3') {
    printf("This image is not in PGM/PPM format.\n");
    exit(EXIT_FAILURE);
  }
  if (c == '6' || c == '3') {
    ncomp = 3;
  }
  if (c == '2' || c == '3') {
    isASCII = true;
  }
  while (status < DONE) {
    c = fgetc(fp);
    // eat spaces, LF or CR, or comments
    while (c == ' ' || c == '\n' || c == 0xd) {
      c = fgetc(fp);
      if (c == '#') {
        fgets(comment, sizeof(comment), fp);
        c = fgetc(fp);
      }
    }
    // get values
    while (c != ' ' && c != '\n' && c != 0xd) {
      val *= 10;
      val += c - '0';
      c = fgetc(fp);
    }

    // update status
    switch (status) {
      case 0:
        width = val;
        val   = 0;
        status++;
        break;
      case 1:
        height = val;
        val    = 0;
        status++;
        break;
      case 2:
        maxval = val;
        val    = 0;
        status++;
        break;
      default:
        break;
    }
  }

  if (maxval > 255) {
    printf("Maximum value greater than 255 is not supported\n");
    exit(EXIT_FAILURE);
  }

  int numpixels = width * height * ncomp;
  auto *buf     = (unsigned char *)malloc(numpixels * sizeof(unsigned char));
  if (buf == nullptr) {
    printf("malloc() error\n");
    exit(EXIT_FAILURE);
  }

  // read pixel values into buffer
  if (!isASCII) {
    fread(buf, sizeof(unsigned char), numpixels, fp);
  } else {
    for (int i = 0; i < numpixels; ++i) {
      val = 0;
      c   = fgetc(fp);
      while (c != ' ' && c != '\n' && c != EOF) {
        val *= 10;
        val += c - '0';
        c = fgetc(fp);
      }
      buf[i] = val;
    }
  }
  fclose(fp);
  return buf;
}