// #include <mobo/jit.h>
#include <mobo/kvm.h>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <sys/types.h>
#include <iostream>


using namespace mobo;


int main(int argc, char *argv[]) {

  int kvmfd = open("/dev/kvm", O_RDWR);

  std::string binary = argv[1];
  // create a vmm
  kvm vmm(kvmfd, 1);
  // give it some ram
  vmm.init_ram(256 * 1024l * 1024l);
  // load the kernel elf
  vmm.load_elf(binary);
  // run the vm
  vmm.run();
  return 0;
}

