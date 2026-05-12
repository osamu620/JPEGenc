# JPEGenc

A fast JPEG-1 baseline encoder in C++17, SIMD-vectorized via [Google Highway](https://github.com/google/highway) with optional multi-threading.

## Features

- Full baseline JPEG encoding (DCT, quantization, Huffman coding)
- SIMD-accelerated pipeline with runtime ISA dispatch (NEON, SSE2, AVX2, AVX-512)
- Single-threaded and multi-threaded modes (producer/consumer with per-strip parallelism)
- All standard chroma subsampling modes: 4:4:4, 4:2:2, 4:1:1, 4:4:0, 4:2:0, 4:1:0, and grayscale
- Quality factor 0--100 (IJG-compatible quantization tables)
- Low memory footprint: line buffers are recycled across strips; no full-image allocation
- Reusable encoder: `invoke()` can be called repeatedly without reallocating internal state

## Performance

Measured on **Apple M3 Max** (single-thread and auto-thread), 4K image (3840 x 2160, 8.3 MP), quality 75, `-b` benchmark mode (2 s warmup + 2 s measurement):

| Mode | Subsampling | Throughput | Frame rate |
|------|-------------|------------|------------|
| 1 thread | 4:2:0 | 696 MP/s | 84 fps |
| 1 thread | 4:4:4 | 486 MP/s | 59 fps |
| 1 thread | GRAY | 965 MP/s | 116 fps |
| auto (16 threads) | 4:2:0 | 3595 MP/s | 433 fps |
| auto (16 threads) | 4:4:4 | 1961 MP/s | 236 fps |

## Memory footprint

The encoder allocates only per-strip line buffers (16 rows), not full-image buffers. Peak RSS for the 4K image above:

| Mode | Peak RSS |
|------|----------|
| 1 thread | ~6 MB |
| auto (16 threads) | ~12 MB |

## Dependencies

- C++17 compiler (Clang, GCC, MSVC)
- [Google Highway](https://github.com/google/highway) (>= 1.0.6) -- included as a git submodule

## Build from source

Clone with submodules:

```sh
git clone https://github.com/osamu620/JPEGenc.git --recursive
cd JPEGenc
```

Build with CMake (Ninja recommended):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -G Ninja -DBUILD_TESTING=OFF
cmake --build build
```

`-DBUILD_TESTING=OFF` suppresses Highway's own test targets.

The build produces:
- `build/bin/libjpegenc_R.{so,dylib,dll}` -- shared library
- `build/bin/jpenc` -- CLI encoder

Build types: `Release` (`-O3`), `Debug` (`-O0 -g -fsanitize=address`, executable named `jpenc_dbg`), `RelWithDebInfo`.

## Usage

```
jpenc -i input.ppm -o output.jpg [options]
```

| Option | Description | Default |
|--------|-------------|---------|
| `-i FILE` | Input PPM/PGM file (required) | |
| `-o FILE` | Output JPEG file (required) | |
| `-q N` | Quality factor (0--100) | 75 |
| `-c MODE` | Chroma subsampling: `444`, `422`, `411`, `440`, `420`, `410`, `GRAY` | `420` |
| `-t N` | Threading: `1` = single-thread, `0` = auto, `N >= 2` = N workers | `1` |
| `-b` | Benchmark mode (2 s warmup + 2 s measurement, reports fps and MP/s) | off |
| `-h` | Print help | |

### Examples

Encode at quality 90 with 4:4:4 subsampling:

```sh
./jpenc -i photo.ppm -o photo.jpg -q 90 -c 444
```

Multi-threaded encoding using all available cores:

```sh
./jpenc -i photo.ppm -o photo.jpg -t 0
```

Run a throughput benchmark:

```sh
./jpenc -i photo.ppm -o photo.jpg -b
```

## Library API

The public header `include/jpegenc.hpp` exposes two classes:

```cpp
#include <jpegenc.hpp>

FILE *fp;
// ... open and parse PPM header to get fpos, width, height, nc ...
jpegenc::im_info input(fp, fpos, width, height, nc);

int qf = 75, ycc = 5; // YUV420
jpegenc::jpeg_encoder encoder(input, qf, ycc, /*num_threads=*/1);
encoder.invoke();
std::vector<uint8_t> jpeg = encoder.get_codestream();
```

The encoder object is reusable -- calling `invoke()` again re-encodes the same image (useful for benchmarking or quality sweeps) without reallocating internal buffers.

## License

See [LICENSE](LICENSE).

