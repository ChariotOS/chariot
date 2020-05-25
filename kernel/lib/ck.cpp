#include <ck/string.h>
#include <ck/vec.h>

#define INIT_STRING       \
  if (m_buf == nullptr) { \
    reserve(16);          \
    m_len = 0;            \
    m_buf[0] = '\0';      \
  }



ck::string::string() {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;

  INIT_STRING;
}

ck::string::string(const char* s) {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;
  INIT_STRING;
  *this = s;
}

ck::string::string(const string& s) {
  m_buf = nullptr;
  m_len = 0;
  m_cap = 0;
  INIT_STRING;
  m_buf[0] = '\0';
  for (char c : s) push(c);
}

ck::string::string(string&& s) {
  // take control of another string's data
  m_len = s.m_len;
  m_cap = s.m_cap;
  m_buf = s.m_buf;

  s.m_len = 0;
  s.m_cap = 0;
  s.m_buf = nullptr;
}

ck::string::string(char c) {
  m_buf = 0;
  m_cap = 0;
  m_len = 0;
  push(c);
}

ck::string::~string() {
  if (m_buf != nullptr) kfree(m_buf);
}

ck::vec<ck::string> ck::string::split(char c, bool include_empty) {
  vec<string> tokens;
  string token = "";

  string& self = *this;

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

unsigned ck::string::len() const { return m_len; }
unsigned ck::string::size() const { return m_len; }

char ck::string::pop() {

	char c = -1;
	if (m_len > 0) {
		m_len--;
		c = m_buf[m_len];
		m_buf[m_len] = 0;
	}

	return c;
}

void ck::string::clear() {
  reserve(16);
  m_buf[0] = '\0';
  m_len = 0;
}

char& ck::string::operator[](unsigned int j) {
  if (j >= m_len) panic("string out of range");
  return m_buf[j];
}

char ck::string::operator[](unsigned int j) const {
  if (j >= m_len) panic("string out of range");
  return m_buf[j];
}

ck::string& ck::string::operator=(const string& s) {
  reserve(s.m_cap);
  m_buf[0] = '\0';
  for (char c : s) push(c);
  return *this;
}

ck::string& ck::string::operator=(const char* s) {
  unsigned int len = strlen(s);
  reserve(len + 1);
  m_len = len;
  memcpy(m_buf, s, len);
  return *this;
}

ck::string& ck::string::operator+=(const string& s) {
  reserve(len() + s.len());
  for (char c : s) push(c);
  return *this;
}
ck::string& ck::string::operator+=(char c) {
  push(c);
  return *this;
}
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
void ck::string::reserve(unsigned int new_cap) {
  if (new_cap < 16) new_cap = 16;
  new_cap = round_up(new_cap, 16);
  if (m_buf == nullptr) {
    assert(m_cap == 0);
    m_buf = (char*)kmalloc(new_cap);
		memset(m_buf, 0, new_cap);

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
void ck::string::push(char c) {


  if (c == '\0') return;
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

ck::string operator+(const ck::string& lhs, const ck::string& rhs) {
  return ck::string(lhs) += rhs;
}

ck::string operator+(const ck::string& lhs, const char* rhs) {
  return ck::string(lhs) += ck::string(rhs);
}

ck::string operator+(const char* lhs, const ck::string& rhs) {
  return ck::string(lhs) += rhs;
}


bool operator==(const ck::string& lhs, const ck::string& rhs) {
  if (lhs.len() != rhs.len()) return false;

  unsigned cap = lhs.len();
  unsigned n = 0;
  while ((n < cap) && (lhs[n] == rhs[n])) n++;
  return (n == cap);
}

bool operator==(const ck::string& lhs, const char* rhs) {
  return (lhs == ck::string(rhs));
}

bool operator==(const char* lhs, const ck::string& rhs) {
  return (ck::string(lhs) == rhs);
}

bool operator>(const ck::string& lhs, const ck::string& rhs) {
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

bool operator>(const ck::string& lhs, const char* rhs) {
  return (lhs > ck::string(rhs));
}
bool operator>(const char* lhs, const ck::string& rhs) {
  return (ck::string(lhs) > rhs);
}
bool operator!=(const ck::string& lhs, const ck::string& rhs) { return !(lhs == rhs); }
bool operator!=(const ck::string& lhs, const char* rhs) { return !(lhs == rhs); }
bool operator!=(const char* lhs, const ck::string& rhs) { return !(lhs == rhs); }
bool operator<(const ck::string& lhs, const ck::string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<(const ck::string& lhs, const char* rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<(const char* lhs, const ck::string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}
bool operator<=(const ck::string& lhs, const ck::string& rhs) { return !(lhs > rhs); }
bool operator<=(const ck::string& lhs, const char* rhs) { return !(lhs > rhs); }
bool operator<=(const char* lhs, const ck::string& rhs) { return !(lhs > rhs); }

bool operator>=(const ck::string& lhs, const ck::string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const ck::string& lhs, const char* rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const char* lhs, const ck::string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

