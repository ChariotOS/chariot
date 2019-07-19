#include <mobo/kvm.h>

#include <fcntl.h>
#include <sys/types.h>

using namespace mobo;

int main(int argc, char *argv[]) {
  int kvmfd = open("/dev/kvm", O_RDWR);

  std::string binary = argv[1];
  // create a vmm
  kvm vmm(kvmfd, 1);
  // give it some ram
  vmm.init_ram(1024 * 1024l * 1024l);
  // load the kernel elf
  vmm.load_elf(binary);
  // run the vm
  vmm.run();
  return 0;
}
