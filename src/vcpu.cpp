#include <mobo/vcpu.h>

using namespace mobo;

std::string vcpu::read_string(u64 gva) {
  std::string str;


  unsigned char buf[8000];


  int read = read_guest(gva, buf, 8000);
  printf("read %d\n", read);


  for (int i = 0; i < read; i++) {
    printf("%02x ", buf[i]);
  }
  printf("\n");

  return str;
}

int vcpu::read_guest(u64 gva, void *buf, size_t len) {
  // guest page number
  u64 gpn = gva >> 12;
  // offset within the page
  int offset = gva & 0xfff;

  u64 hpn;

  // host page number
  void *found = translate_address(gpn << 12);
  if (found == nullptr) return 0;
  hpn = ((u64)found) >> 12;

  int i;
  for (i = 0; i < len; i++) {
    // get the physical address
    char *pa = (char*)((hpn << 12) + offset);

    // copy the byte
    ((char*)buf)[i] = *pa;

    offset++;
    // handle moving to the next page
    if (offset >= 4096) {
      gpn += 1;
      offset = 0;

      found = translate_address(gpn << 12);
      if (found == nullptr) return i;
      hpn = ((u64)found) >> 12;
    }
  }

	return i;
}

