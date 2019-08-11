#include <string.h>
#include <printk.h>

string::string() {
  length = 0;
  data = new char[0];
}

string::string(char c) {
  length = 1;
  data = new char(c);
}

string::string(const char* c) {
  if (c) {
    unsigned n = 0;
    while (c[n] != '\0') n++;
    length = n;
    data = new char[n];
    for (unsigned j = 0; j < n; j++) data[j] = c[j];
  } else {
    length = 0;
    data = new char[0];
  }
}

string::string(const string& s) {
  length = s.len();
  data = new char[length];
  for (unsigned j = 0; j < length; j++) data[j] = s[j];
}

string::~string() { delete[] data; }

unsigned string::len() const { return length; }

int string::index(char c) const {
  for (unsigned j = 0; j < length; j++)
    if (data[j] == c) return (int)j;
  return -1;
}

char string::operator[](unsigned j) const {
  if (j >= length) panic("string out of range");
  return data[j];
}

char& string::operator[](unsigned j) {
  if (j >= length) panic("string out of range");
  return data[j];
}

string& string::operator=(const string& s) {
  if (this == &s) return *this;

  delete data;
  length = s.len();
  data = new char[length];
  for (unsigned j = 0; j < length; j++) data[j] = s[j];
  return *this;
}

string& string::operator+=(const string& s) {
  unsigned len = length + s.len();
  char* str = new char[len];

  for (unsigned j = 0; j < length; j++) str[j] = data[j];

  for (unsigned i = 0; i < s.len(); i++) str[length + i] = s[i];

  delete data;
  length = len;
  data = str;
  return *this;
}

string operator+(const string& lhs, const string& rhs) {
  return string(lhs) += rhs;
}

string operator+(const string& lhs, char rhs) {
  return string(lhs) += string(rhs);
}

string operator+(const string& lhs, const char* rhs) {
  return string(lhs) += string(rhs);
}

string operator+(char lhs, const string& rhs) { return string(lhs) += rhs; }
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

bool operator==(const string& lhs, char rhs) { return (lhs == string(rhs)); }

bool operator==(const string& lhs, const char* rhs) {
  return (lhs == string(rhs));
}

bool operator==(char lhs, const string& rhs) { return (string(lhs) == rhs); }

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

bool operator>(const string& lhs, char rhs) { return (lhs > string(rhs)); }

bool operator>(const string& lhs, const char* rhs) {
  return (lhs > string(rhs));
}

bool operator>(char lhs, const string& rhs) { return (string(lhs) > rhs); }

bool operator>(const char* lhs, const string& rhs) {
  return (string(lhs) > rhs);
}

bool operator!=(const string& lhs, const string& rhs) { return !(lhs == rhs); }

bool operator!=(const string& lhs, char rhs) { return !(lhs == rhs); }

bool operator!=(const string& lhs, const char* rhs) { return !(lhs == rhs); }

bool operator!=(char lhs, const string& rhs) { return !(lhs == rhs); }

bool operator!=(const char* lhs, const string& rhs) { return !(lhs == rhs); }

bool operator<(const string& lhs, const string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}

bool operator<(const string& lhs, char rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}

bool operator<(const string& lhs, const char* rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}

bool operator<(char lhs, const string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}

bool operator<(const char* lhs, const string& rhs) {
  return !(lhs == rhs) && !(lhs > rhs);
}

bool operator<=(const string& lhs, const string& rhs) { return !(lhs > rhs); }

bool operator<=(const string& lhs, char rhs) { return !(lhs > rhs); }

bool operator<=(const string& lhs, const char* rhs) { return !(lhs > rhs); }

bool operator<=(char lhs, const string& rhs) { return !(lhs > rhs); }

bool operator<=(const char* lhs, const string& rhs) { return !(lhs > rhs); }

bool operator>=(const string& lhs, const string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const string& lhs, char rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const string& lhs, const char* rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(char lhs, const string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

bool operator>=(const char* lhs, const string& rhs) {
  return (lhs == rhs) || (lhs > rhs);
}

