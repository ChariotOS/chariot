// #include <mobo/jit.h>
#include <mobo/kvm.h>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <signal.h>

#include <fcntl.h>

using namespace mobo;

int run_vm(std::string path) {

  static int kvmfd = open("/dev/kvm", O_RDWR);


  // create a vmm
  kvm vmm(kvmfd, 1);
  // give it some ram (256 mb)
  vmm.init_ram(256 * 1024l * 1024l);

  try {

    while (true) {
      // load the kernel elf
      vmm.load_elf(path);

      auto start = std::chrono::high_resolution_clock::now();
      // run the vm
      vmm.run();

      auto end = std::chrono::high_resolution_clock::now();

      auto dur = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
      printf("%ldus\n", dur.count());
      vmm.reset();
    }
  } catch (std::exception &ex) {
    printf("ex: %s\n", ex.what());
    return -1;
  }

  return 0;
}

// haskell FFI function
extern "C" int mobo_run_vm(char *binary) { return run_vm(binary); }

