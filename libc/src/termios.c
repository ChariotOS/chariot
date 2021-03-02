#include <sys/ioctl.h>
#include <termios.h>


pid_t tcgetpgrp(int fd) {
  return ioctl(fd, TIOCGPGRP);
}

int tcsetpgrp(int fd, pid_t pgid) {
  return ioctl(fd, TIOCSPGRP, pgid);
}
