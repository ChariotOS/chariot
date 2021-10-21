#include <syscall.h>
#include <cpu.h>
#include <fs.h>

int sys::ioctl(int fd, int cmd, unsigned long arg) {
  int n = -1;

  ck::ref<fs::File> file = curproc->get_fd(fd);
  if (file) {
    n = file->ioctl(cmd, arg);
  }
  return n;
}
