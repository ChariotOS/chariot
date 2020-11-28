#pragma once

#include <stddef.h>

namespace ck {
  template <typename T, int N>
  class array {
   public:
    typedef array __self;
    typedef T value_type;
    typedef value_type& reference;
    typedef const value_type& const_reference;
    typedef value_type* iterator;
    typedef const value_type* const_iterator;
    typedef value_type* pointer;
    typedef const value_type* const_pointer;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;



    // iterators:
    iterator begin() { return iterator(data()); }
    const_iterator begin() const { return const_iterator(data()); }
    iterator end() { return iterator(data() + N); }
    const_iterator end() const { return const_iterator(data() + N); }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }


    value_type* data() { return this->elems; }
    const value_type* data() const { return this->elems; }

    constexpr size_type size() const { return N; }
    size_type max_size() const { return N; }
    constexpr bool empty() const { return false; }


    reference operator[](size_type __n) { return this->elems[__n]; }
    const_reference operator[](size_type __n) const { return this->elems[__n]; }


    void fill(const value_type& val) {
      for (size_t i = 0; i < N; i++) elems[i] = val;
    }

   private:
    T elems[N];
  };
}  // namespace ck
