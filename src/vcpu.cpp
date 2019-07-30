#include <mobo/vcpu.h>

using namespace mobo;

std::string vcpu::read_string(u64 gva) {
  std::string str;
  guest_array<char> mem(this, gva);

  try {
    for (int i = 0; true; i++) {
      char c = mem[i];
      if (c == '\0') break;
      str.push_back(c);
    }
  } catch (std::exception &e) {
    return str;
  }
  return str;
}

int vcpu::read_guest(u64 gva, void *buf, size_t len) {
  auto mem = guest_array<char>(this, gva);
  int i = 0;
  try {
    for (i = 0; i < len; i++) {
      ((char *)buf)[i] = mem[i];
    }
  } catch (std::exception &e) {
    // catch an exception that occurs when the virtual address isn't mapped
    return i;
  }
  return i;
}

