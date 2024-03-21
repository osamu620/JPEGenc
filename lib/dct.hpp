#pragma once

#include <cstdint>
#include <vector>

HWY_BEFORE_NAMESPACE();
namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
void dct2_core(int16_t *HWY_RESTRICT data);
}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy
HWY_AFTER_NAMESPACE();