// #include <mobo/jit.h>
#include <mobo/kvm.h>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>

#include <fcntl.h>

using namespace mobo;

int run_vm(std::string path) {
  static int kvmfd = open("/dev/kvm", O_RDWR);

  // int i = 0;

  try {

    // create a vmm
    kvm vmm(kvmfd, 1);
    // give it some ram (256 mb)
    vmm.init_ram(1024 * 1024l * 1024l);

    // load the kernel elf
    vmm.load_elf(path);

    // auto start = std::chrono::high_resolution_clock::now();

    // run the vm
    vmm.run();

    /*
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    printf("%d,%ld\n", i++, dur.count());
    */

    vmm.reset();
  } catch (std::exception &ex) {
    fprintf(stderr, "ex: %s\n", ex.what());
    return -1;
  }

  return 0;
}

// haskell FFI function
extern "C" int mobo_run_vm(char *binary) { return run_vm(binary); }

