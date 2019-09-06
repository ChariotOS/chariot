#pragma once

#include <template_lib.h>
#include <vec.h>

class string_checker;

class string {
 protected:
  friend string_checker;
  u32 m_len = 0;
  u32 m_cap = 0;
  char* m_buf = nullptr;

 public:
  // iterator magic
  typedef char* iterator;
  typedef const char* const_iterator;
  inline char* begin() { return m_buf; }
  char* end() { return m_buf + m_len; }
  const char* begin() const { return m_buf; }
  const char* end() const { return m_buf + m_len; }

  string();
  string(const char*);
  string(const string&);
  string(string&&);
  string(char);

  ~string(void);
  vec<string> split(char c, bool include_empty = false);

  char& operator[](u32 index);
  char operator[](u32 index) const;

  void reserve(u32);

  void clear(void);

  u32 len(void) const;
  u32 size(void) const;

  inline const char* get() const { return m_buf; }

  string& operator=(const string& s);
  string& operator=(const char* s);

  string& operator+=(char c);
  string& operator+=(const string& s);

  void push(char c);


  // implemented in printk.cpp
  static string format(const char *fmt, ...);
  int scan(const char *fmt, ...);
};

string operator+(const string& lhs, const string& rhs);
string operator+(const string& lhs, const char* rhs);
string operator+(const char* lhs, const string& rhs);
bool operator==(const string& lhs, const string& rhs);
bool operator==(const string& lhs, const char* rhs);
bool operator==(const char* lhs, const string& rhs);
bool operator>(const string& lhs, const string& rhs);
bool operator>(const string& lhs, const char* rhs);
bool operator>(const char* lhs, const string& rhs);
bool operator!=(const string& lhs, const string& rhs);
bool operator!=(const string& lhs, const char* rhs);
bool operator!=(const char* lhs, const string& rhs);
bool operator<(const string& lhs, const string& rhs);
bool operator<(const string& lhs, const char* rhs);
bool operator<(const char* lhs, const string& rhs);
bool operator<=(const string& lhs, const string& rhs);
bool operator<=(const string& lhs, const char* rhs);
bool operator<=(const char* lhs, const string& rhs);
bool operator>=(const string& lhs, const string& rhs);
bool operator>=(const string& lhs, const char* rhs);
bool operator>=(const char* lhs, const string& rhs);



template <>
struct Traits<string> : public GenericTraits<string> {
  static u64 hash(const string &s) {
    const i64 p = 31;
    const i64 m = 1e9 + 9;
    i64 hash_value = 0;
    i64 p_pow = 1;
    for (char c : s) {
        hash_value = (hash_value + (c - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
  }
};
