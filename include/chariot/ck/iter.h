#pragma once

#ifdef KERNEL
#include <types.h>
#else
#include <stdlib.h>
#endif

namespace ck {
  template <typename T>
  class buf_iter {
   public:
    class iterator {
     public:
      bool operator!=(const iterator& other) const { return m_index != other.m_index; }
      bool operator==(const iterator& other) const { return m_index == other.m_index; }
      bool operator<(const iterator& other) const { return m_index < other.m_index; }
      bool operator>(const iterator& other) const { return m_index > other.m_index; }
      bool operator>=(const iterator& other) const { return m_index >= other.m_index; }
      iterator& operator++() {
        ++m_index;
        return *this;
      }
      iterator& operator--() {
        --m_index;
        return *this;
      }
      iterator operator-(int value) { return {m_buf, m_index - value}; }
      iterator operator+(int value) { return {m_buf, m_index + value}; }
      iterator& operator=(const iterator& other) {
        m_index = other.m_index;
        return *this;
      }
      T& operator*() { return m_buf[m_index]; }
      int operator-(const iterator& other) { return m_index - other.m_index; }

      bool is_end() const { return m_index == m_buf.size(); }
      int index() const { return m_index; }

      iterator(buf_iter<T>& b, int index) : m_buf(b), m_index(index) {}

     private:
      buf_iter<T>& m_buf;
      int m_index{0};
    };

    buf_iter(T* buf, size_t sz) : m_buf(buf), m_sz(sz) {}

    iterator begin(void) { return iterator(*this, 0); }
    iterator end(void) { return iterator(*this, m_sz); }

		size_t size(void) const { return m_sz; }
    T& operator[](int i) { return m_buf[i]; }

   private:
    T* m_buf;
    size_t m_sz;
  };
}  // namespace ck
