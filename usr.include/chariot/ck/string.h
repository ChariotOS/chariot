
#pragma once

#include <template_lib.h>
#include "vec.h"


namespace ck {

class string_checker;

class string {
 protected:
  friend string_checker;
  unsigned int m_len = 0;
  unsigned int m_cap = 0;
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

	/**
	 * This is meant to read from buffers like `char buf[SIZE];`
	 * where the construction call would be...
	 *
	 * ck::string s(buf, SIZE);
	 */
	inline string(char *c, size_t len) {
		for (size_t i = 0; i < len; i++) {
			if (c[i] == 0) break;
			push(c[i]);
		}
	}

  ~string(void);
  vec<string> split(char c, bool include_empty = false);

  char& operator[](unsigned int index);
  char operator[](unsigned int index) const;

  void reserve(unsigned int);

  void clear(void);

  unsigned int len(void) const;
  unsigned int size(void) const;

	// remove a char from the end
	char pop(void);

  inline const char* get() const {
		m_buf[len()] = 0; // null terminate
		return m_buf;
	}

  string& operator=(const string& s);
  string& operator=(const char* s);

  string& operator+=(char c);
  string& operator+=(const string& s);

  void push(char c);


  // implemented in printk.cpp
  static string format(const char *fmt, ...);
  int scan(const char *fmt, ...);
};


ck::string operator+(const ck::string& lhs, const ck::string& rhs);
// ck::string operator+(const ck::string& lhs, const char* rhs);
// ck::string operator+(const char* lhs, const ck::string& rhs);
bool operator==(const ck::string& lhs, const ck::string& rhs);
// bool operator==(const ck::string& lhs, const char* rhs);
// bool operator==(const char* lhs, const ck::string& rhs);
bool operator>(const ck::string& lhs, const ck::string& rhs);
// bool operator>(const ck::string& lhs, const char* rhs);
// bool operator>(const char* lhs, const ck::string& rhs);
bool operator!=(const ck::string& lhs, const ck::string& rhs);
// bool operator!=(const ck::string& lhs, const char* rhs);
// bool operator!=(const char* lhs, const ck::string& rhs);
bool operator<(const ck::string& lhs, const ck::string& rhs);
// bool operator<(const ck::string& lhs, const char* rhs);
// bool operator<(const char* lhs, const ck::string& rhs);
bool operator<=(const ck::string& lhs, const ck::string& rhs);
// bool operator<=(const ck::string& lhs, const char* rhs);
// bool operator<=(const char* lhs, const ck::string& rhs);
bool operator>=(const ck::string& lhs, const ck::string& rhs);
// bool operator>=(const ck::string& lhs, const char* rhs);
// bool operator>=(const char* lhs, const ck::string& rhs);

}





template <>
struct Traits<ck::string> : public GenericTraits<ck::string> {
  static unsigned long long hash(const ck::string &s) {
    const long long p = 31;
    const long long m = 1e9 + 9;
    long long hash_value = 0;
    long long p_pow = 1;
    for (char c : s) {
        hash_value = (hash_value + (c - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return hash_value;
  }
};
