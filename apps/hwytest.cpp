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

using T = int16_t;

//        0,  1,  8,  16, 9,  2,  3,  10,   0, 8,  1, 9,  2, 10, 3, 11
//        17, 24, 32, 25, 18, 11, 4,  5,    4, 12, 5, 13, 6, 14, 7, 15
//        12, 19, 26, 33, 40, 48, 41, 34,
//        27, 20, 13, 6,  7,  14, 21, 28,
//        35, 42, 49, 56, 57, 50, 43, 36,
//        29, 22, 15, 23, 30, 37, 44, 51,
//        58, 59, 52, 45, 38, 31, 39, 46,
//        53, 60, 61, 54, 47, 55, 62, 63

// clang-format off
alignas(16) int16_t indices[]  =  {
        0,  1,  8,  0,  9,  2,  3, 10,
        0,  0,  0,  0,  0, 11,  4,  5,
        1,  8,  0,  9,  2,  0,  0,  0,
        0,  0,  0,  1,  8,  0,  9,  2,
        11,  4,  0,  0,  0,  0,  5, 12,
        0,  0, 13,  6,  7, 14,  0,  0,
        3, 10,  0,  0,  0,  0, 11,  4,
        0,  0,  1,  8,  9,  2,  0,  0,
        13,  6,  0,  7, 14,  0,  0,  0,
        0,  0,  0, 13,  6,  0,  7, 14,
        10, 11,  4,  0,  0,  0,  0,  0,
        5, 12, 13,  6,  0,  7, 14, 15
    };
// clang-format on
// Alternative to per-function HWY_ATTR: see HWY_BEFORE_NAMESPACE
HWY_ATTR void Func(T* HWY_RESTRICT array) {
  const hn::ScalableTag<T> s16;
  auto v0 = hn::Load(s16, array);
  auto v1 = hn::Load(s16, array + 8);
  auto v2 = hn::Load(s16, array + 16);
  auto v3 = hn::Load(s16, array + 24);
  auto v4 = hn::Load(s16, array + 32);
  auto v5 = hn::Load(s16, array + 40);
  auto v6 = hn::Load(s16, array + 48);
  auto v7 = hn::Load(s16, array + 56);

  auto row0   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[0 * 8]));
  row0        = InsertLane(row0, 3, ExtractLane(v2, 0));
  auto row1   = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[1 * 8]));
  auto row1_1 = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[2 * 8]));
  auto row2   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[3 * 8]));
  auto row3   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[4 * 8]));
  auto row3_1 = TwoTablesLookupLanes(s16, v0, v1, SetTableIndices(s16, &indices[5 * 8]));
  auto row4   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[6 * 8]));
  auto row4_1 = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[7 * 8]));
  auto row5   = TwoTablesLookupLanes(s16, v2, v3, SetTableIndices(s16, &indices[8 * 8]));
  auto row6   = TwoTablesLookupLanes(s16, v4, v5, SetTableIndices(s16, &indices[9 * 8]));
  auto row6_1 = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[10 * 8]));
  auto row7   = TwoTablesLookupLanes(s16, v6, v7, SetTableIndices(s16, &indices[11 * 8]));
  row7        = InsertLane(row7, 4, ExtractLane(v5, 7));

  auto m5                       = FirstN(s16, 5);
  auto m3                       = FirstN(s16, 3);
  alignas(16) uint8_t mask34[8] = {0b00111100};
  auto m34                      = LoadMaskBits(s16, mask34);

  row1   = IfThenZeroElse(m5, row1);
  row1_1 = IfThenElseZero(m5, row1_1);
  row1   = Or(row1, row1_1);
  row1   = InsertLane(row1, 2, ExtractLane(v4, 0));
  row2   = IfThenZeroElse(m3, row2);
  row2   = InsertLane(row2, 0, ExtractLane(v1, 4));
  row2   = InsertLane(row2, 1, ExtractLane(v2, 3));
  row2   = InsertLane(row2, 2, ExtractLane(v3, 2));
  row2   = InsertLane(row2, 5, ExtractLane(v6, 0));
  row3   = IfThenZeroElse(m34, row3);
  row3_1 = IfThenElseZero(m34, row3_1);
  row3   = Or(row3, row3_1);
  row4   = IfThenZeroElse(m34, row4);
  row4_1 = IfThenElseZero(m34, row4_1);
  row4   = Or(row4, row4_1);
  row5   = IfThenZeroElse(Not(m5), row5);
  row5   = InsertLane(row5, 2, ExtractLane(v1, 7));
  row5   = InsertLane(row5, 5, ExtractLane(v4, 5));
  row5   = InsertLane(row5, 6, ExtractLane(v5, 4));
  row5   = InsertLane(row5, 7, ExtractLane(v6, 3));
  row6   = IfThenZeroElse(m3, row6);
  row6_1 = IfThenElseZero(m3, row6_1);
  row6   = Or(row6, row6_1);
  row6   = InsertLane(row6, 5, ExtractLane(v3, 7));
  /* DCT block is now in zig-zag order; start Huffman encoding process. */

  /* Construct bitmap to accelerate encoding of AC coefficients.  A set bit
   * means that the corresponding coefficient != 0.
   */
  auto ttt = Not(Lt(HighestSetBitIndex(row0), Set(s16, 0)));
  int a    = 1;

  hn::Store(row0, s16, array);
  hn::Store(row1, s16, array + 8);
  hn::Store(row2, s16, array + 16);
  hn::Store(row3, s16, array + 24);
  hn::Store(row4, s16, array + 32);
  hn::Store(row5, s16, array + 40);
  hn::Store(row6, s16, array + 48);
  hn::Store(row7, s16, array + 56);
}

}  // namespace HWY_NAMESPACE
}  // namespace project

// The table of pointers to the various implementations in HWY_NAMESPACE must
// be compiled only once (foreach_target #includes this file multiple times).
// HWY_ONCE is true for only one of these 'compilation passes'.
#if HWY_ONCE

namespace project {

// This macro declares a static array used for dynamic dispatch.
HWY_EXPORT(Func);

void CallFunc(int16_t* HWY_RESTRICT array) {
  // This must reside outside of HWY_NAMESPACE because it references (calls the
  // appropriate one from) the per-target implementations there.
  // For static dispatch, use HWY_STATIC_DISPATCH.
  return HWY_DYNAMIC_DISPATCH(Func)(array);
}

}  // namespace project
#endif  // HWY_ONCE

int main() {
  constexpr size_t N = 64;
  alignas(16) int16_t data[N];

  for (size_t i = 0; i < N; ++i) {
    data[i] = i % 2;
  }

  // data = a * data + b
  project::CallFunc(data);
  for (size_t i = 0; i < N; ++i) {
    if (i % 8 == 0) std::cout << std::endl;
    std::cout << data[i] << " ";
  }
}