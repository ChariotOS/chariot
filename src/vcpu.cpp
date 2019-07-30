#include <mobo/vcpu.h>

using namespace mobo;

template <typename T>
class guest_array {
 private:
  u64 base = 0;

  // cached information. if the gpn is the same, then the ppn is valid.
  // if not, then we need to find the new mapping
  u64 gpn = -1;
  u64 ppn = -1;
  mobo::vcpu *cpu = nullptr;

 public:
  guest_array(mobo::vcpu *cpu, u64 base) : cpu(cpu), base(base) {}

  T &operator[](size_t off) {
    // the guest virtual address with the offset
    auto gva = base + off;
    size_t baseoff = base & 0xfff;
    // if the guest page number doesnt match, we need to look it up
    if (gpn != gva >> 12) {
      gpn = gva >> 12;
      void *found = cpu->translate_address(gpn << 12);
      if (found == nullptr) throw std::runtime_error("borked");
      ;
      ppn = ((u64)found >> 12);
    }

    int ppn_offset = off % 4096;
    u64 addr = ((ppn << 12) + ppn_offset + baseoff);
    return *(T *)addr;
  }
};

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
  guest_array<char> mem(this, gva);
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

