// #include <mobo/jit.h>
#include <mobo/kvm.h>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <map>

using namespace mobo;

template <typename T>
class range_lookup {
  struct range {
    u64 start, end;
  };

  struct comparator {
    bool operator()(const u64& v, const range& r) const { return v < r.start; }
    bool operator()(const range& r, const u64& v) const { return r.end <= v; }
    bool operator()(const range& lhs, const range& rhs) const {
      return lhs.end <= rhs.start;
    }
    using is_transparent = std::true_type;
  };

  std::map<range, T, comparator> map;

 public:
  void add(u64 start, u64 len, T val) {
    struct range r = {.start = start, .end = start + len};
    map[r] = val;
  }

  T lookup(u64 addr) {
    auto it = map.find(addr);

    if (it == map.end()) {
      throw std::runtime_error("not found");
    }
    return it->second;
  }
};

int run_vm(std::string path) {
  static int kvmfd = open("/dev/kvm", O_RDWR);

  /*
  range_lookup<int> rn;

  rn.add(0, 10, 1);
  rn.add(10, 10, 2);

  for (int i = 0; i < 20; i++) {
    printf("%d %d\n", i, rn.lookup(i));
  }

  exit(0);
  */

  try {
    // create a vmm
    kvm vmm(kvmfd, 1);
    vmm.init_ram(8 * 1024l * 1024l * 1024l);
    // load the kernel elf
    vmm.load_elf(path);
    // auto start = std::chrono::high_resolution_clock::now();
    vmm.run();
    /*
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start);
    printf("%d,%ld\n", i++, dur.count());
    */
    vmm.reset();
  } catch (std::exception& ex) {
    fprintf(stderr, "ex: %s\n", ex.what());
    return -1;
  }

  return 0;
}

// haskell FFI function
extern "C" int mobo_run_vm(char* binary) { return run_vm(binary); }

