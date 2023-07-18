//
// Created by OSAMU WATANABE on 2023/07/13.
//
// Generates code for every target that this compiler can support.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwytest.cpp"  // this file
#include <hwy/foreach_target.h>           // must come before highway.h
#include <hwy/highway.h>
#include <iostream>

namespace project {
namespace HWY_NAMESPACE {  // required: unique per target

// Can skip hn:: prefixes if already inside hwy::HWY_NAMESPACE.
namespace hn = hwy::HWY_NAMESPACE;

using T = int;

// Alternative to per-function HWY_ATTR: see HWY_BEFORE_NAMESPACE
HWY_ATTR void MulAddLoop(const T* HWY_RESTRICT mul_array, const T* HWY_RESTRICT add_array,
                         const size_t size, T* HWY_RESTRICT x_array) {
  const hn::ScalableTag<T> d;
  for (size_t i = 0; i < size; i += hn::Lanes(d) * 2) {
    auto v0 = hn::Undefined(d);
    auto v1 = hn::Undefined(d);
    auto a0 = hn::Undefined(d);
    auto a1 = hn::Undefined(d);
    auto b0 = hn::Undefined(d);
    auto b1 = hn::Undefined(d);

    hn::LoadInterleaved2(d, x_array + i, v0, v1);
    hn::LoadInterleaved2(d, mul_array + i, a0, a1);
    hn::LoadInterleaved2(d, add_array + i, b0, b1);

    v0 = hn::MulAdd(a0, v0, b0);
    v1 = hn::MulAdd(a1, v1, b1);

    hn::Store(v0, d, x_array + i);
    hn::Store(v1, d, x_array + i + hn::Lanes(d));
    //    const auto mul = hn::Load(d, mul_array + i);
    //    const auto add = hn::Load(d, add_array + i);
    //    auto x         = hn::Load(d, x_array + i);
    //    x              = hn::MulAdd(mul, x, add);
    //    hn::Store(x, d, x_array + i);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace project

// The table of pointers to the various implementations in HWY_NAMESPACE must
// be compiled only once (foreach_target #includes this file multiple times).
// HWY_ONCE is true for only one of these 'compilation passes'.
#if HWY_ONCE

namespace project {

// This macro declares a static array used for dynamic dispatch.
HWY_EXPORT(MulAddLoop);

void CallMulAddLoop(const int* HWY_RESTRICT mul_array, const int* HWY_RESTRICT add_array, const size_t size,
                    int* HWY_RESTRICT x_array) {
  // This must reside outside of HWY_NAMESPACE because it references (calls the
  // appropriate one from) the per-target implementations there.
  // For static dispatch, use HWY_STATIC_DISPATCH.
  return HWY_DYNAMIC_DISPATCH(MulAddLoop)(mul_array, add_array, size, x_array);
}

}  // namespace project
#endif  // HWY_ONCE

int main() {
  constexpr size_t N = 64;
  alignas(16) int a[N];
  alignas(16) int b[N];
  alignas(16) int data[N];

  for (size_t i = 0; i < N; ++i) {
    data[i] = i;
    a[i]    = 1;
    b[i]    = 0;
  }

  // data = a * data + b
  project::CallMulAddLoop(a, b, N, data);
  for (size_t i = 0; i < N; ++i) {
    std::cout << data[i] << " " << std::endl;
  }
}