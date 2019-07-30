#include <mobo/vcpu.h>

using namespace mobo;

std::string vcpu::read_string(u64 gva) {
  std::string str;


  unsigned char buf[100];


  int read = read_guest(gva, buf, 100);
  printf("read %d\n", read);


  for (int i = 0; i < 100; i++) {
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

  // host page number
  u64 hpn = ((u64)translate_address(gpn << 12)) >> 12;

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
      hpn = ((u64)translate_address(gpn << 12)) >> 12;
      printf("%lx\n", hpn);
    }
  }

	return i;
}

