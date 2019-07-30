#include <mobo/dev_mgr.h>

#include <mobo/vcpu.h>
#include <stdint.h>
#include <mutex>

#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

class posix_interface : public mobo::device {
 private:
 public:
  virtual std::vector<mobo::port_t> get_ports(void) { return {0xfafa}; }

  virtual int in(mobo::vcpu *, mobo::port_t port, int sz, void *data) {
    return -1;
  }

  virtual int out(mobo::vcpu *cpu, mobo::port_t port, int sz, void *data) {
    int result = 0;
    mobo::regs regs;

    cpu->read_regs(regs);

    // the virtual address of the arguments is stored in rdi (the first argument
    // in c's calling convention)
    u64 arg_gva = regs.rdi;

    u64 *args = (u64 *)cpu->translate_address(arg_gva);
    // if the translation failed (nullptr), then return -1 in rax
    if (args == nullptr) {
      printf("failed\n");
      result = -1;
      goto do_return;
    }

    try {
      // open
      if (args[0] == 0x05) {
        std::string filename = cpu->read_string(args[1]);
        int flags = args[2];
        int options = args[3];
        printf("filename: %s\n", filename.c_str());
        printf("flags: %d\n", flags);
        printf("options: %o\n", options);


        // result = open(filename, flags, options);
        goto do_return;
      }

    } catch (...) {
      result = -1;
      goto do_return;
    }

    result = -1;

  do_return:

    regs.rax = result;
    cpu->write_regs(regs);
    return 0;
  }
};

MOBO_REGISTER_DEVICE(posix_interface);
