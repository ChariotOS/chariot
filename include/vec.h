#ifndef __VEC_H__
#define __VEC_H__

#include <asm.h>
#include <mem.h>
#include <printk.h>
#include <types.h>

template <typename T>
class vec {
 protected:
  u32 len = 0;
  u32 cap = 0;
  T *m_data = nullptr;

 public:
  typedef T *iterator;
  typedef const T *const_iterator;

  T *begin() { return m_data; }
  T *end() { return m_data + len; }
  const T *begin() const { return m_data; }
  const T *end() const { return m_data + len; }

  inline vec() { reserve(1); }

  inline ~vec() {
    if (m_data != nullptr) {
      // go through and call the dtor on all the elements
      for (auto &el : *this) {
        el.~T();
      }
      kfree(m_data);
    }
  }

  inline explicit vec(int count) {
    reserve(count);
    len = cap;
  }

  inline vec(const vec<T> &o) {
    reserve(o.capacity());
    *this = o;
  }

  // copy assignment
  inline vec &operator=(const vec<T> &v) {
    reserve(v.capacity());
    clear();
    for (auto &e : v) {
      push(e);
    }
    return *this;
  }

  // move assignment
  inline vec &operator=(vec<T> &&v) {
    m_data = v.m_data;
    len = v.len;
    cap = v.cap;

    v.m_data = nullptr;
    v.len = 0;
    v.cap = 0;
    return *this;
  }

  // access specified element with bounds checking
  inline T &at(int i) {
    if (!(i < size())) {
      panic("Vector out of range!\n");
      // throw std::out_of_range("out of range access into slice");
    }
    return m_data[i];
  }

  // access specified element w/o bounds checking
  inline T &operator[](int i) const { return m_data[i]; }
  // direct access to the underlying array
  const T *data(void) const { return m_data; }

  inline void clear(void) {
    for (auto &e : *this) {
      e.~T();
    }
    len = 0;
    memset(m_data, 0, sizeof(T) * cap);
  }

  inline bool empty(void) const { return len == 0; }

  // returns the number of elements
  inline int size(void) const { return len; }

  // returns the number of free spaces
  inline int capacity(void) const { return cap; }

  // returns a reference to the first element
  inline T &front(void) const { return m_data[0]; }
  // returns a reference to the last element
  inline T &back(void) const { return m_data[len - 1]; }

  inline void reserve(u32 new_cap) {
    if (m_data == nullptr) {
      m_data = (T *)kmalloc(sizeof(T) * new_cap);
    } else {
      if (new_cap <= cap) return;
      m_data = (T *)krealloc(m_data, sizeof(T) * new_cap);
    }

    cap = new_cap;
  }

  inline void push(const T &value) {
    if (len == cap) {
      if (cap < 10) {
        reserve(10);
      } else
        reserve(cap * 2);
    }
    // printk("");
    m_data[len++] = value;
  }

  inline void push(T &&value) {
    // printk("this one\n");
    if (len >= cap) {
      if (cap < 10) {
        reserve(10);
      } else
        reserve(cap * 2);
    }
    // printk("%p\n", m_data);
    m_data[len++] = move(value);
  }

  inline void pop(void) {
    if (len > 0) len--;
  }
};

#endif
