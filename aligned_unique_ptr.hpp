#pragma once
#include <memory>

/********************************************************************************
 * aligned unique pointer
 *******************************************************************************/

// deleter
template <class T>
struct delete_aligned {
  void operator()(T *data) const {
#if defined(_MSC_VER)
    _aligned_free(data);
#elif defined(__MINGW32__) || defined(__MINGW64__)
    __mingw_aligned_free(data);
#else
    std::free(data);
#endif
  }
};

// allocator
template <class T>
using unique_ptr_aligned = std::unique_ptr<T, delete_aligned<T>>;
template <class T>
unique_ptr_aligned<T> aligned_uptr(size_t align, size_t size) {
  // return unique_ptr_aligned<T>(static_cast<T *>(aligned_mem_alloc(size * sizeof(T), align)));
  return unique_ptr_aligned<T>(static_cast<T *>(aligned_alloc(align, size * sizeof(T))));
}
