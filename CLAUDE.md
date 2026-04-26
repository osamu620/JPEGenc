# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

JPEGenc is a C++17 JPEG-1 encoder. SIMD acceleration is delivered through Google
Highway (a git submodule at `thirdparty/highway`), which must be present —
clone with `--recursive` or run `git submodule update --init --recursive`
before building.

## Build

CMake out-of-tree build, with Ninja recommended:

```
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja -DBUILD_TESTING=OFF
cmake --build build
```

- `-DBUILD_TESTING=OFF` suppresses Highway's own test targets; the project
  itself has no test suite, and `CMakeLists.txt` re-asserts this with
  `add_definitions("-DBUILD_TESTING=OFF")`.
- Outputs land in `build/bin/`. The library is `jpegenc_R` (Release) or
  `jpegenc_d` (Debug); the CLI is `jpenc` (Release) or `jpenc_dbg` (Debug).
- Debug builds enable AddressSanitizer (`-fsanitize=address`) and `-O0 -g`.
- Release/RelWithDebInfo use `-O3`. On x86_64 GCC/Clang, `-march=native
  -mtune=native` is forced; on aarch64, Apple builds target `apple-m3`,
  others target `cortex-a72 / armv8-a`. MSVC uses `/arch:AVX2` (x86_64) or
  `/arch:armv8.2` (aarch64).
- `HWY_DISABLED_TARGETS=HWY_NEON_BF16` is set globally. To force a non-SIMD
  build for debugging, uncomment one of the `HWY_COMPILE_ONLY_EMU128` /
  `HWY_COMPILE_ONLY_SCALAR` definitions in `CMakeLists.txt`.

## Run

```
build/bin/jpenc -i input.ppm -o output.jpg [-q 0..100] [-c 444|422|420|440|411|410|GRAY] [-b]
```

`-b` runs a benchmark loop (2 s warmup + 2 s measurement) and reports fps and
MP/s. Default quality is 75; default subsampling is 4:2:0. Input must be a
PNM/PPM file (parser in `apps/pnm.hpp`).

## Code style

`.clang-format` is Google-based, 2-space indent, 108-column limit,
`AlignConsecutiveAssignments: true`, `SortIncludes: false` (do not let an
editor re-order includes — Highway's `foreach_target.h` must precede
`highway.h`).

## Architecture

### Public API (`include/jpegenc.hpp`)

The only public surface is the `jpegenc::jpeg_encoder` class with a pImpl
indirection to `jpeg_encoder_impl` (defined in `lib/jpegenc.cpp`). Callers
build a `jpegenc::im_info` from a `FILE*` plus dimensions, construct the
encoder with a quality factor and a `YCCtype` (see `lib/ycctype.hpp`), call
`invoke()`, and retrieve the buffer via `get_codestream()`.

### Pipeline (`lib/jpegenc.cpp::jpeg_encoder_impl::invoke`)

Processing is striped in 16-line MCU-row chunks (`BUFLINES = 16`,
`DCTSIZE = 8` from `constants.hpp`):

1. `imchunk` (lib/image_chunk.hpp) reads `BUFLINES` rows from the input file
   into an aligned, width-rounded staging buffer (`yuv0`).
2. `rgb2ycbcr` (color.cpp) converts to planar Y/Cb/Cr in place across the
   strip, when `ncomp == 3`.
3. `subsample` (color.cpp) downsamples chroma into `yuv1` according to the
   `YCCtype` selected; `YCC_HV[YCCtype]` in `ycctype.hpp` encodes the
   horizontal/vertical sampling factors as packed nibbles.
4. `encode_lines` (block_coding.cpp) dispatches per-MCU work: forward DCT
   (`dct2_core` from dct.cpp) → quantization (`quantize_core` from
   quantization.cpp) → Huffman + run/level coding (`encode_block`) → bitstream
   (`bitstream.cpp`).
5. After the main loop, the residual `< 16`-line tail is handled with the
   same calls but a smaller `mcu_height`. `bitstream::finalize()` returns the
   completed codestream.

The main header (SOI, APP0/JFIF, DQT, SOF0, DHT, SOS) is emitted by
`create_mainheader` in `jpgheaders.cpp`; quantization tables come from
`create_scaled_qtable` (`qmatrix.hpp`); zig-zag order and the standard
Huffman tables live in `zigzag_order.hpp` and `huffman_tables.hpp`. RST
markers are emitted only when `use_RESET` is true (currently hard-coded to
`false`).

### SIMD dispatch model

Highway's runtime dispatch is the central organizing pattern. `block_coding.cpp`
and `color.cpp` follow the same template:

```
#define HWY_TARGET_INCLUDE "<this file>"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy { namespace HWY_NAMESPACE { ... } }
HWY_AFTER_NAMESPACE();
#if HWY_ONCE
namespace jpegenc_hwy { HWY_EXPORT(fn); void wrapper(...) { HWY_DYNAMIC_DISPATCH(fn)(...); } }
#endif
```

Highway re-includes the file once per supported target ISA and the
`HWY_DYNAMIC_DISPATCH` wrapper picks the best one at runtime. Because of
this, `dct.cpp` and `quantization.cpp` are not compiled standalone — they are
`#include`d *inside* the per-target namespace of `block_coding.cpp` so their
intrinsics specialize to the same target. They are intentionally **not**
listed in `lib/CMakeLists.txt` as separate translation units, and adding them
there will cause duplicate-symbol/link errors.

`block_coding.cpp` further selects a per-ISA-width implementation of the
zero-bitmap / nbits-extraction step by `#include`ing one of
`block_coding_512.cpp` (AVX-512), `block_coding_256.cpp` (AVX2),
`block_coding_128.cpp` (SSE/NEON), or `block_coding_scalar.cpp` (no SIMD).
These files are fragments meant to be included inside `encode_block` — not
standalone compilation units. When changing block-coding logic, edit each
width variant.

### Color/subsampling matrix

`YCC` (`ycctype.hpp`) is the canonical enum; `parse_args.hpp` redefines the
same enum locally, so any new format must be added in **both** places and
also in `YCC_HV`. `GRAY` and `GRAY2` differ in CLI semantics: a 1-component
input is internally promoted to `YCC::GRAY`, while `-c GRAY` on a 3-component
input forces `YCC::GRAY2` (color planes discarded).

### Bitstream

`bitstream.cpp/.hpp` owns a 64-bit accumulator plus an 8-byte staging area
and an exponentially-grown output `stream_buf`. JPEG byte-stuffing (insert
`0x00` after every emitted `0xFF`) and RST marker insertion are handled here.
`USE_VECTOR` is currently 0 — the buffer is a manually managed
`unique_ptr<uint8_t[]>`, not a `std::vector`.
