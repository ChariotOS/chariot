#include <mobo/dev_mgr.h>

#include <mobo/vcpu.h>
#include <stdint.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <mutex>
#include <unordered_map>

class posix_interface : public mobo::device {
 private:
  std::unordered_map<int, int> fd_alias;

  int add_next_fd(int fd) {
    for (int i = 0; true; i++) {
      if (fd_alias.count(i) == 0) {
        fd_alias[i] = fd;
        return i;
      }
    }
    return -1;
  }

 public:
  virtual std::vector<mobo::port_t> get_ports(void) { return {0xfafa}; }

  virtual void init(void) {
    fd_alias.clear();

    fd_alias[fileno(stdin)] = fileno(stdin);
    fd_alias[fileno(stdout)] = fileno(stdout);
    fd_alias[fileno(stderr)] = fileno(stderr);
  }

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
      // write
      if (args[0] == 0x04) {
        // read virtual fd
        int vfd = args[1];
        u64 data_gva = args[2];
        size_t count = args[3];
        // unfortunately, to write we need to copy the data into a buffer that
        // linux can understand
        char *buf = new char[count];
        mobo::guest_array<char> mem(cpu, data_gva);

        for (int i = 0; i < count; i++) buf[i] = mem[i];

        if (fd_alias.count(vfd) == 0) {
          result = -1;
          goto do_return;
        }

        result = ::write(fd_alias[vfd], buf, count);
        delete[] buf;
        goto do_return;
      }

      // open
      if (args[0] == 0x05) {
        // read the string from the virtual machine
        std::string filename = cpu->read_string(args[1]);
        int fd = ::open(filename.c_str(), args[2], args[3]);
        result = add_next_fd(fd);
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
