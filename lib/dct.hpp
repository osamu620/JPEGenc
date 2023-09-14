#pragma once

#include <cstdint>
#include <vector>

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
HWY_ATTR void dct2_core(int16_t *HWY_RESTRICT data);
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy