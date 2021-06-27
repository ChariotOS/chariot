#pragma once
// this is a stub for std:: in c++

#ifdef __cplusplus

#include <stddef.h>
// #include <ini
// #include "bits/stdc++.h"

namespace std {
  template <typename T>
  class initializer_list {
   private:
    T *_data;
    size_t _size;

    constexpr initializer_list(const T *data, size_t size) : _data(data), _size(size) {}

   public:
    size_t size() const { return _size; }

    constexpr initializer_list() : _data(0), _size(0) {}

    const T &operator[](size_t index) const { return _data[index]; }

    T &operator[](size_t index) { return _data[index]; }

    constexpr const T *begin() const { return _data; }

    constexpr const T *end() const { return _data + _size; }
  };
}  // namespace std

#endif