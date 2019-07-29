// #include <mobo/jit.h>
#include <mobo/kvm.h>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>

using namespace mobo;

extern "C" int mobo_square(int x) {
  printf("hello, I am a nice meme\n");
  return x * x;
}

int run_vm(std::string path) {
  static int kvmfd = open("/dev/kvm", O_RDWR);

  try {
    // create a vmm
    kvm vmm(kvmfd, 1);
    // give it some ram (256 mb)
    vmm.init_ram(256 * 1024l * 1024l);
    // load the kernel elf
    vmm.load_elf(path);
    // run the vm
    vmm.run();
  } catch (std::exception &ex) {
    printf("ex: %s\n", ex.what());
    return -1;
  }

  return 0;
}

extern "C" int mobo_run_vm(char *binary) { return run_vm(binary); }

