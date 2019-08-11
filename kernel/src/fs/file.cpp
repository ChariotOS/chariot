#include <fs/file.h>
#include <printk.h>

fs::path::path(string s) { parse(s); }

void fs::path::parse(string &s) {
  m_is_root = s[0] == '/';
  parts = s.split('/');
}

string fs::path::to_string(void) {
  string s;

  if (m_is_root) {
    s += '/';
  }

  for (int i = 0; i < len(); i++) {
    s += parts[i];
    if (i != len() - 1) {
      s += '/';
    }
  }
  return s;
}
