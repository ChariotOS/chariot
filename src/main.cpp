#include <mobo/kvmdriver.h>
#include <mobo/machine.h>

#include <fcntl.h>
#include <sys/types.h>

using namespace mobo;

int main(int argc, char *argv[]) {
  int kvmfd = open("/dev/kvm", O_RDWR);

  // create a kvm driver with 1 cpu
  kvmdriver k(kvmfd, 1);

  // create a machine to house the driver
  machine m(k);

  // 256 MiB of ram
  m.allocate_ram(256 * 1024l * 1024l);
  // load the kernel elf
  m.load_elf(argv[1]);
  // run the vm
  m.run();
  return 0;
}
