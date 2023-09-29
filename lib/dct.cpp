// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "dct.cpp"  // this file
#include <hwy/foreach_target.h>       // must come before highway.h
#include <hwy/highway.h>

#include <utility>

#include "dct.hpp"
#include "constants.hpp"
#include "ycctype.hpp"

namespace jpegenc_hwy {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

}  // namespace HWY_NAMESPACE
}  // namespace jpegenc_hwy
