# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

JPEG-1 baseline encoder in C++17, SIMD-vectorized via [Google Highway](https://github.com/google/highway) and optionally multi-threaded via the bundled `BS::thread_pool` (`lib/BS_thread_pool.hpp`). Highway is a git submodule under `thirdparty/highway`.

The encoder runs in either of two modes, selected by the `num_threads` constructor parameter (or the `-t` CLI flag):
- **Single-thread (`num_threads == 1`, default):** strips are processed sequentially and written directly into one `bitstream`; no RST markers, no thread pool, lowest per-invoke overhead.
- **Multi-thread (`num_threads >= 2` or `0` = auto):** producer/consumer with `min(num_threads, hardware_concurrency, num_strips)` workers; strips encode into per-strip bitstreams that are concatenated at the end, with RST markers between strips so each strip is independently decodable.

The build produces a shared library `libjpegenc` and a CLI `jpenc` (release) / `jpenc_dbg` (debug) under `${BUILD_DIR}/bin/`.

## Build

Highway must be checked out before configuring. After a fresh clone or branch switch:

```sh
git submodule update --init --recursive
```

CMake (Ninja recommended). Always pass `-DBUILD_TESTING=OFF` so Highway's own test targets aren't generated:

```sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja -DBUILD_TESTING=OFF
cmake --build build-release
```

Build types:
- `Release` — `-O3 -DNDEBUG`. Library suffixed `_R`, executable named `jpenc`.
- `Debug` — `-O0 -g -fsanitize=address`. Library suffixed `_d`, executable named `jpenc_dbg`.
- `RelWithDebInfo` — `-O3 -g -DNDEBUG`.

Architecture flags are set automatically: `-march=native` on x86_64, `armv8-a` (with `-mtune=native` on Apple, `cortex-a72` elsewhere) on aarch64. GCC additionally gets `-flax-vector-conversions`.

To restrict Highway to a single ISA when debugging vector code, uncomment one of the `add_definitions(...)` lines at the top of `CMakeLists.txt`:
- `HWY_COMPILE_ONLY_SCALAR` — disable all SIMD targets.
- `HWY_COMPILE_ONLY_EMU128` — emulated 128-bit only.
- `HWY_DISABLED_TARGETS=HWY_NEON_BF16` (or any other target) — exclude specific targets.

## Run / smoke-test

There is no formal test suite. Validation is done by encoding the sample images and checking PSNR against the source. From `${BUILD_DIR}/bin/`:

```sh
./jpenc -i input.ppm -o out.jpg [-q 0..100] [-c 444|422|411|440|420|410|GRAY] [-t N] [-b]
```

`-t N` selects threading: `1` (default) is the single-thread fast path, `0` auto-picks `hardware_concurrency()`, and `N >= 2` requests that many workers (capped to `min(hardware_concurrency, num_strips)`). `-b` enables benchmark mode (2 s warmup + 2 s measurement, reports fps and MP/s). Default quality is 75, default subsampling is 4:2:0.

`do.sh` and `do_mono.sh` (in repo root) sweep all chroma subsampling modes and run ImageMagick `compare -metric PSNR` against the source — copy or symlink them next to `jpenc` and call `./do.sh basename QF` (where `basename.ppm` exists).

## Architecture

### Public surface
`include/jpegenc.hpp` is the only public header. It exposes `jpegenc::im_info` (input descriptor wrapping a `FILE*`, owns the file handle) and `jpegenc::jpeg_encoder`. The encoder is a thin pimpl over `jpeg_encoder_impl` in `lib/jpegenc.cpp`. Everything else under `lib/` is private.

### Encoder lifecycle
`jpeg_encoder_impl`'s constructor is the heavyweight setup point. It picks `num_workers` (clamped to `min(num_threads, hardware_concurrency, num_strips)`, with `0 → hardware_concurrency`), allocates `num_workers` `worker_buffers` sets (input + planar Y/Cb/Cr line buffers — recycled across strips and across invokes), builds the Huffman tables (`tab_Y.init<0>()` / `tab_C.init<1>()`), and computes the scaled quantization table. The `BS::thread_pool`, the per-strip output bitstreams, and the free-buffer queue are allocated only when `num_workers > 1`. These all depend only on fixed image params (width, height, ncomp, QF, YCCtype, num_threads) and are reused across every `invoke()` call.

`invoke()` dispatches to one of two private bodies based on `num_workers`:

- **`invoke_st()` (`num_workers == 1`):** writes the JPEG main header into the shared `bitstream enc`, then loops `num_strips = ceil(rounded_height / BUFLINES)` times — reading each strip into the single shared input buffer and pipelining it through color → subsample → DCT/quantize/Huffman, all written directly into `enc`. After the strips, `enc.finalize()` flushes pending bits, appends EOI, and returns the codestream as a `std::vector<uint8_t>`. The `bitstream`'s internal buffer is rewound to pos=0 inside `finalize()`, so the next invocation reuses the allocation. **No RST markers.**

- **`invoke_mt()` (`num_workers > 1`):** the main thread acts as a producer — for each strip it acquires a free worker buffer (blocking on the cv if all are in flight), reads the strip into that buffer, then dispatches a pool task that runs the encode + an RST marker into a per-strip `bitstream`. After `pool->wait()`, the codestream is assembled in one pass directly into the caller's vector: cached header bytes + each `strip_cs[s]` + EOI. **RST_n marker between strips** keeps each strip independently decodable, which is what makes per-strip parallelism legal.

`use_RESET = (num_workers > 1)` — controlled internally by the constructor, never read from outside.

### Encoding pipeline
The image is processed in horizontal strips of `BUFLINES = 16` rows (see `lib/constants.hpp`). Per strip, in `encode_strip()`:

1. `imchunk::get_lines_from(row_off, input_buf)` (`lib/image_chunk.hpp`) — `fread` 16 rows into the temporary buffer, replicate-pad right edge to `rounded_width` (multiple of `DCTSIZE * H_max` and of `HWY_MAX_BYTES`) and bottom edge to a full strip. Tracks `expected_pos` to skip the per-call `fseek` when reads are sequential (the common case) — `fseek` invalidates the FILE's internal buffer and is not free. Always called from the producer (main) thread, never inside a worker, so no synchronization is needed on the `imchunk`.
2. `jpegenc_hwy::rgb2ycbcr` (`lib/color.cpp`) — interleaved RGB → planar Y/Cb/Cr (uint8). Skipped for grayscale input.
3. `jpegenc_hwy::subsample` (`lib/color.cpp`) — chroma decimation per `YCCtype` and level-shift by 128 (output is int16).
4. `jpegenc_hwy::encode_lines` → `encode_mcus` (`lib/block_coding.cpp`) — for each MCU: forward DCT (`dct2_core` in `lib/dct.cpp`), quantize (`quantize_core` in `lib/quantization.cpp`), then `encode_block` does zigzag+run-length+Huffman directly into the strip's `bitstream` (the shared `enc` in ST, `strip_cs[s]` in MT).
5. `mcu_height` for the last strip is `min(BUFLINES, rounded_height - row_off)`, which correctly handles `chroma_v=1` modes whose `rounded_height` is a multiple of 8 but not 16.

### Highway / SIMD pattern
`color.cpp` and `block_coding.cpp` are the two foreach-target translation units. Each follows the standard Highway recipe:

```cpp
#define HWY_TARGET_INCLUDE "<this file>"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>
HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy { namespace HWY_NAMESPACE {
  // ... HWY_ATTR functions ...
}}
HWY_AFTER_NAMESPACE();
#if HWY_ONCE
namespace jpegenc_hwy {
  HWY_EXPORT(<entry_fn>);
  void <public_fn>(...) { HWY_DYNAMIC_DISPATCH(<entry_fn>)(...); }
}
#endif
```

Public dispatchers (`encode_lines`, `rgb2ycbcr`, `subsample`) live at the bottom of these files inside `#if HWY_ONCE`. Runtime ISA selection is `HWY_DYNAMIC_DISPATCH`.

`dct.cpp` and `quantization.cpp` are **not** standalone foreach-target units — they are `#include`d inside the `HWY_NAMESPACE` block of `block_coding.cpp` so each target gets its own specialization. They are still listed in `lib/CMakeLists.txt` (and so also compile as top-level TUs), but only the included copies are reached by `HWY_DYNAMIC_DISPATCH`.

The four files `block_coding_128.cpp`, `block_coding_256.cpp`, `block_coding_512.cpp`, `block_coding_scalar.cpp` are **not** in `lib/CMakeLists.txt` and must not be added. They are conditionally `#include`d inside `encode_block` based on `HWY_TARGET` (`<= HWY_AVX3` → 512, `<= HWY_AVX2` → 256, else NEON/SSE/etc → 128, scalar fallback elsewhere). Each provides a vector-width-specific implementation of "zigzag-reorder + per-coefficient nbits + populate `bitmap`/`dp`/`bits` arrays" that the surrounding Huffman emitter consumes. When editing one width, check whether the same change applies to the others.

In `block_coding_128.cpp` the `#if USE_ARM_SPECIFIC && ((HWY_TARGET == HWY_NEON) || (HWY_TARGET == HWY_NEON_WITHOUT_AES))` block uses native `vqtbl4q_s8`/`vsetq_lane_s16` intrinsics (the libjpeg-turbo zigzag pattern). The Highway-only `#else` branch is much heavier on lane-crossing ops (`TwoTablesLookupLanes` × 12 plus many `InsertLane`/`ExtractLane`), so on Apple Silicon the intrinsic path is the one to keep working. The `#if` parens matter — without them, operator precedence (`&&` binds tighter than `||`) makes `USE_ARM_SPECIFIC` only gate the NEON arm.

### YCC subsampling table
`YCC` enum and `YCC_HV[8][2]` (`lib/ycctype.hpp`) encode H/V sampling factors as packed nibbles (`0xHV`). Throughout the code, `>> 4` extracts H, `& 0xF` extracts V. `GRAY` is single-component-in-3-component-input; `GRAY2` is true single-component output. Keep this enum stable — `YCC_HV` is indexed directly by it and the parser in `apps/parse_args.hpp` maps CLI strings to it.

### Bitstream
`lib/bitstream.hpp` accumulates bits in a 64-bit register and flushes whole qwords with byte-stuffing (`0xFF` → `0xFF 0x00`). The fast path uses `JPEGENC_BSWAP64` and detects "any 0xFF byte present" via the SWAR pattern `d & 0x8080... & ~(d + 0x0101...)`. `put_RST(n)` flushes, then writes the RST marker (no byte-stuffing, since markers are not in the entropy-coded segment).

`finalize()` flushes pending bits, appends EOI, copies the contents into a `std::vector<uint8_t>` (one memcpy of the whole codestream), and resets the underlying `stream_buf` to `pos=0` so the same `bitstream` instance is ready to be written from scratch on the next `invoke()`. `bits` and `tmp` are also reset by the flush at the start of `finalize()`. This implicit reset is what makes `bitstream enc;` a member work across invocations.

The compile-time switch `USE_VECTOR` in `bitstream.hpp` toggles between a `stream_buf` (raw `unique_ptr<uint8_t[]>` with manual doubling) and a `std::vector<uint8_t>` backing store. Default is `stream_buf`. If you change a `bitstream` API, update both branches.

## Conventions

- Formatting: clang-format (Google base, 2-space indent, column 108, `SortIncludes: false`, `IndentPPDirectives: BeforeHash`). `thirdparty/highway/*` is excluded via `.clang-format-ignore` — never reformat third-party code.
- Headers: prefer `lib/*.hpp` includes from inside `lib/`; the public `include/jpegenc.hpp` is the *only* header an external consumer should include.
- Allocations: long-lived buffers use `hwy::AllocateAligned<T>(N)` (`hwy::AlignedFreeUniquePtr<T[]>`) so vector loads/stores stay aligned. Stack scratch uses `HWY_ALIGN`.
- The CLZ/BSWAP intrinsics are wrapped as `JPEGENC_CLZ32/64` and `JPEGENC_BSWAP64` in `lib/constants.hpp`. On platforms where `bswap64` is unavailable, `BSWAP64_UNAVAILABLE` is defined and `bitstream.cpp::send_8_bytes` provides the fallback — keep both paths working.
