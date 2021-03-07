
#pragma once

#include <template_lib.h>
#include "vec.h"

namespace ck {

  class string_checker;



  template <typename T>
  class basic_string_view {
   protected:
    const T* m_start;
    size_t m_len;

   public:
    // iterator magic
    typedef const T* iterator;
    typedef const T* const_iterator;
    inline const char* begin() {
      return m_start;
    }
    const T* end() {
      return m_start + m_len;
    }
    const T* begin() const {
      return m_start;
    }
    const T* end() const {
      return m_start + m_len;
    }

    basic_string_view(const T* start, size_t len) : m_start(start), m_len(len) {
    }

    basic_string_view(T* start) : m_start(start) {
      for (m_len = 0; start[m_len] != 0; m_len++) {
      }
    }

    basic_string_view(const T* start) : m_start(start) {
      for (m_len = 0; start[m_len] != 0; m_len++) {
      }
    }
    // no destructor, since we don't own anything
    const char* get(void) const {
      return m_start;
    }
    inline size_t len(void) const {
      return m_len;
    }

    operator T*(void) {
      return m_start;
    }

    inline T operator[](unsigned int index) const {
      return m_start[index];
    }


    inline basic_string_view<T> substring_view(off_t start, size_t len) const {
      return basic_string_view<T>(m_start + start, len);
    }
  };




  template <typename T>
  class basic_string {
   protected:
    unsigned int m_len = 0;
    unsigned int m_cap = 0;
    T* m_buf = nullptr;

   public:
    // iterator magic
    typedef T* iterator;
    typedef const T* const_iterator;
    inline T* begin() {
      return m_buf;
    }
    T* end() {
      return m_buf + m_len;
    }
    const T* begin() const {
      return m_buf;
    }
    const T* end() const {
      return m_buf + m_len;
    }



#define INIT_STRING       \
  if (m_buf == nullptr) { \
    reserve(16);          \
    m_len = 0;            \
    m_buf[0] = '\0';      \
  }

    basic_string() {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;

      INIT_STRING;
    }
    constexpr basic_string(const T* s) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      *this = s;
    }


    constexpr basic_string(T* s) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      *this = s;
    }
    basic_string(const basic_string& s) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      m_buf[0] = '\0';
      for (T c : s)
        push(c);
    }
    basic_string(basic_string&& s) {
      // take control of another string's data
      m_len = s.m_len;
      m_cap = s.m_cap;
      m_buf = s.m_buf;

      s.m_len = 0;
      s.m_cap = 0;
      s.m_buf = nullptr;
    }


    basic_string(T c) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      push(c);
    }


    inline basic_string(T* c, size_t len) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      for (size_t i = 0; i < len; i++) {
        if (c[i] == 0) break;
        push(c[i]);
      }
    }
    inline basic_string(const T* c, size_t len) {
      m_buf = nullptr;
      m_len = 0;
      m_cap = 0;
      INIT_STRING;
      for (size_t i = 0; i < len; i++) {
        if (c[i] == 0) break;
        push(c[i]);
      }
    }

    basic_string(basic_string_view<T>& s) {
      basic_string(s.get(), s.len());
    }
    basic_string(const basic_string_view<T>& s) {
      basic_string(s.get(), s.len());
    }


    ~basic_string(void) {
      if (m_buf != nullptr) free(m_buf);
    }


    ck::vec<basic_string<T>> split(T c, bool include_empty = false) const {
      ck::vec<basic_string<T>> tokens;
      basic_string token = "";

      const basic_string& self = *this;

      for (unsigned int i = 0; i < len(); i++) {
        if (self[i] == c) {
          if (token.len() == 0 && !include_empty) {
            continue;
          } else {
            tokens.push(token);
            token = "";
            continue;
          }
        } else {
          token += self[i];
        }
      }

      if (token.len() != 0 || include_empty) {
        tokens.push(token);
      }

      return tokens;
    }


    inline basic_string_view<T> substring_view(off_t start, size_t len) const {
      return basic_string_view<T>(m_buf + start, len);
    }


    operator basic_string_view<T>(void) {
      return substring_view(0, len());
    }



    inline basic_string<T> substring(off_t start, off_t end) const {
      basic_string<T> s;
      for (int i = start; i < end; i++)
        s += (*this)[i];
      return s;
    }


    size_t len() const {
      return m_len;
    }
    size_t size() const {
      return m_len;
    }

    T pop() {
      T c = -1;
      if (m_len > 0) {
        m_len--;
        c = m_buf[m_len];
        m_buf[m_len] = 0;
      }

      return c;
    }

    void clear() {
      reserve(16);
      m_buf[0] = '\0';
      m_len = 0;
    }

    T& operator[](unsigned int j) {
      if (j >= m_len) panic("string out of range");
      return m_buf[j];
    }

    T operator[](unsigned int j) const {
      if (j >= m_len) panic("string out of range");
      return m_buf[j];
    }


    inline const T* get() const {
      if (m_buf != NULL) m_buf[len()] = 0;  // null terminate
      return m_buf;
    }


    basic_string& operator=(const basic_string& s) {
      reserve(s.m_cap);
      m_buf[0] = '\0';
      for (T c : s)
        push(c);
      return *this;
    }

    static void foo() {
    }

    basic_string& operator=(const T* s) {
      if (s == NULL) return *this;
      unsigned int len = strlen(s);
      reserve(len + 1);
      m_len = len;
      memcpy(m_buf, s, len);
      return *this;
    }

    basic_string& operator+=(const basic_string& s) {
      reserve(len() + s.len());
      for (T c : s)
        push(c);
      return *this;
    }

    basic_string& operator+=(T c) {
      push(c);
      return *this;
    }

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
    void reserve(unsigned int new_cap) {
      if (new_cap < 16) new_cap = 16;
      new_cap = round_up(new_cap, 16);
      if (m_buf == nullptr) {
        assert(m_cap == 0);
        m_buf = (T*)malloc(new_cap * sizeof(T));
        memset(m_buf, 0, new_cap);

        assert(m_buf != nullptr);
      } else {
        if (new_cap <= m_cap) return;
        m_buf = (T*)realloc(m_buf, new_cap * sizeof(T));
      }
      m_cap = new_cap;
    }


    /**
     * main append function (assures null at end)
     */
    void push(T c) {
      if (c == '\0') return;
      INIT_STRING;

      if (m_len + 2 >= m_cap) reserve(m_cap * 2);

      m_buf[m_len++] = c;
      m_buf[m_len] = 0;
    }




    // implemented in printk.cpp
    static basic_string format(const char* fmt, ...);
    int scan(const char* fmt, ...);


    friend ck::basic_string<T> operator+(const ck::basic_string<T>& lhs,
                                         const ck::basic_string<T>& rhs) {
      return basic_string(lhs) += rhs;
    }


    friend ck::basic_string<T> operator+(const ck::basic_string<T>& lhs, const T* rhs) {
      ck::basic_string<T> res = lhs;
      for (int i = 0; rhs[i] != 0; i++)
        res.push(rhs[i]);
      return res;
    }

    friend ck::basic_string<T> operator+(const ck::basic_string<T>& lhs, T* rhs) {
      ck::basic_string<T> res = lhs;
      for (int i = 0; rhs[i] != 0; i++)
        res.push(rhs[i]);
      return res;
    }

    friend bool operator==(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      if (lhs.len() != rhs.len()) return false;

      unsigned cap = lhs.len();
      unsigned n = 0;
      while ((n < cap) && (lhs[n] == rhs[n]))
        n++;
      return (n == cap);
    }


    friend bool operator==(const ck::basic_string<T>& lhs, const T* rhs) {
      size_t rlen = 0;
      for (; rhs[rlen] != 0; rlen++) {
      }
      if (lhs.len() != rlen) return false;

      unsigned cap = lhs.len();
      unsigned n = 0;
      while ((n < cap) && (lhs[n] == rhs[n]))
        n++;
      return (n == cap);
    }


    friend bool operator==(const ck::basic_string<T>& lhs, T* rhs) {
      size_t rlen = 0;
      for (; rhs[rlen] != 0; rlen++) {
      }
      if (lhs.len() != rlen) return false;

      unsigned cap = lhs.len();
      unsigned n = 0;
      while ((n < cap) && (lhs[n] == rhs[n]))
        n++;
      return (n == cap);
    }



    friend bool operator!=(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      return !(lhs == rhs);
    }


    friend bool operator>(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      unsigned cap = (lhs.len() < rhs.len()) ? lhs.len() : rhs.len();
      unsigned n = 0;
      while ((n < cap) && (lhs[n] == rhs[n]))
        n++;
      if (n == cap) return (lhs.len() > rhs.len());

      if ((('A' <= lhs[n] && lhs[n] <= 'Z') || ('a' <= lhs[n] && lhs[n] <= 'z')) &&
          (('A' <= rhs[n] && rhs[n] <= 'Z') || ('a' <= rhs[n] && rhs[n] <= 'z'))) {
        T A = (lhs[n] & ~32);
        T B = (rhs[n] & ~32);
        if (A != B) return (A > B);
      }
      return lhs[n] > rhs[n];
    }


    friend bool operator<(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      return !(lhs == rhs) && !(lhs > rhs);
    }


    friend bool operator<=(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      return !(lhs > rhs);
    }


    friend bool operator>=(const ck::basic_string<T>& lhs, const ck::basic_string<T>& rhs) {
      return (lhs == rhs) || (lhs > rhs);
    }
#undef INIT_STRING
  };

  using string_view = basic_string_view<char>;
  using string = basic_string<char>;




}  // namespace ck




template <typename T>
struct Traits<ck::basic_string<T>> : public GenericTraits<ck::basic_string<T>> {
  static unsigned long long hash(const ck::basic_string<T>& s) {
    const long long p = 31;
    const long long m = 1e9 + 9;
    long long hash_value = 0;
    long long p_pow = 1;
    for (T c : s) {
      hash_value = (hash_value + (c - 'a' + 1) * p_pow) % m;
      p_pow = (p_pow * p) % m;
    }
    return hash_value;
  }
};
