#include <printk.h>
#include <string.h>

#define INIT_STRING       \
  if (m_buf == nullptr) { \
    reserve(16);          \
    m_len = 0;            \
    m_buf[0] = '\0';      \
  }

// #define STR_DEBUG

#ifdef STR_DEBUG
#define CHECK string_checker chk(*this, __LINE__, __PRETTY_FUNCTION__);
#else
#define CHECK
#endif

class string_checker {
  string& s;
  int line;
  const char* func;

 public:
  string_checker(string& s, int line, const char* func)
      : s(s), line(line), func(func) {
    printk("+ (%4d %s): %p\n", line, func, &s);
  }

  ~string_checker() {
    printk("- [%4d %s]: %p\n", line, func, &s);
    /*
    if (s.m_buf == nullptr) {
      assert(s.m_cap == 0);
      assert(s.m_len == 0);
    } else {
      assert(s.m_cap > s.m_len);
    }
    */
  }
};

string::string() {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;

  INIT_STRING;
}

string::string(const char* s) {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;
  INIT_STRING;
  *this = s;
}

string::string(const string& s) {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;
  INIT_STRING;
  CHECK;
  m_buf[0] = '\0';
  for (char c : s) push(c);
}

string::string(string&& s) {
  CHECK;
  // take control of another string's data
  m_len = s.m_len;
  m_cap = s.m_cap;
  m_buf = s.m_buf;

  s.m_len = 0;
  s.m_cap = 0;
  s.m_buf = nullptr;
}

string::string(char c) {
  CHECK;
  m_buf = 0;
  m_cap = 0;
  m_len = 0;
  push(c);
}

string::~string() {
  if (m_buf != nullptr) kfree(m_buf);
}

vec<string> string::split(char c, bool include_empty) {
  CHECK;
  vec<string> tokens;
  string token = "";

  string& self = *this;

  for (u32 i = 0; i < len(); i++) {
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

unsigned string::len() const { return m_len; }
unsigned string::size() const { return m_len; }

void string::clear() {
  CHECK;
  reserve(16);
  m_buf[0] = '\0';
  m_len = 0;
}

char& string::operator[](u32 j) {
  CHECK;
  if (j >= m_len) panic("string out of range");
  return m_buf[j];
}

char string::operator[](u32 j) const {
  if (j >= m_len) panic("string out of range");
  return m_buf[j];
}

string& string::operator=(const string& s) {
  CHECK;
  reserve(s.m_cap);
  m_buf[0] = '\0';
  for (char c : s) push(c);
  return *this;
}

string& string::operator=(const char* s) {
  CHECK;
  u32 len = 0;
  for (; s[len] != '\0'; len++) {
  }
  reserve(len + 1);
  m_len = len;
  memcpy(m_buf, s, len);
  return *this;
}

string& string::operator+=(const string& s) {
  CHECK;
  reserve(len() + s.len());
  for (char c : s) push(c);
  return *this;
}
string& string::operator+=(char c) {
  CHECK;
  push(c);
  return *this;
}

void string::reserve(u32 new_cap) {
  CHECK;
  if (new_cap < 16) new_cap = 16;
  if (m_buf == nullptr) {
    // assert(m_cap == 0);
    m_buf = (char*)kmalloc(new_cap);

    assert(m_buf != nullptr);
  } else {
    if (new_cap <= m_cap) return;
    m_buf = (char*)krealloc(m_buf, new_cap);
  }
  // printk("%p\n", m_buf);
  m_cap = new_cap;
}

/**
 * main append function (assures null at end)
 */
void string::push(char c) {
  CHECK;
  INIT_STRING;

  if (m_len + 2 >= m_cap) reserve(m_cap * 2);

  m_buf[m_len++] = c;
  m_buf[m_len] = 0;
}

/**
 *
 *
 *
 *  standalone operators on strings
 *
 *
 *
 */

string operator+(const string& lhs, const string& rhs) {
  return string(lhs) += rhs;
}

string operator+(const string& lhs, const char* rhs) {
  return string(lhs) += string(rhs);
}

string operator+(const char* lhs, const string& rhs) {
  return string(lhs) += rhs;
}

bool operator==(const string& lhs, const string& rhs) {
  if (lhs.len() != rhs.len()) return false;

  unsigned cap = lhs.len();
  unsigned n = 0;
  while ((n < cap) && (lhs[n] == rhs[n])) n++;
  return (n == cap);
}

bool operator==(const string& lhs, const char* rhs) {
  return (lhs == string(rhs));
}

bool operator==(const char* lhs, const string& rhs) {
  return (string(lhs) == rhs);
}

bool operator>(const string& lhs, const string& rhs) {
  unsigned cap = (lhs.len() < rhs.len()) ? lhs.len() : rhs.len();
  unsigned n = 0;
  while ((n < cap) && (lhs[n] == rhs[n])) n++;
  if (n == cap) return (lhs.len() > rhs.len());

  if ((('A' <= lhs[n] && lhs[n] <= 'Z') || ('a' <= lhs[n] && lhs[n] <= 'z')) &&
      (('A' <= rhs[n] && rhs[n] <= 'Z') || ('a' <= rhs[n] && rhs[n] <= 'z'))) {
    char A = (lhs[n] & ~32);
    char B = (rhs[n] & ~32);
    if (A != B) return (A > B);
  }
  return lhs[n] > rhs[n];
}

bool operator>(const string& lhs, const char* rhs) {
  return (lhs > string(rhs));
}
bool operator>(const char* lhs, const string& rhs) {
  return (string(lhs) > rhs);
}
bool operator!=(const string& lhs, const string& rhs) { return !(lhs == rhs); }
bool operator!=(const string& lhs, const char* rhs) { return !(lhs == rhs); }
bool operator!=(const char* lhs, const string& rhs) { return !(lhs == rhs); }
bool operator<(const string& lhs, const string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<(const string& lhs, const char* rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<(const char* lhs, const string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<=(const string& lhs, const string& rhs) { return !(lhs > rhs); }
bool operator<=(const string& lhs, const char* rhs) { return !(lhs > rhs); }
bool operator<=(const char* lhs, const string& rhs) { return !(lhs > rhs); }

bool operator>=(const string& lhs, const string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const string& lhs, const char* rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const char* lhs, const string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

