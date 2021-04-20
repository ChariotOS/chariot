#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>

extern int errno;

pid_t tcgetpgrp(int fd) {
  return ioctl(fd, TIOCGPGRP);
}

int tcsetpgrp(int fd, pid_t pgid) {
  return ioctl(fd, TIOCSPGRP, pgid);
}


int tcgetattr(int fd, struct termios* t) {
  return ioctl(fd, TCGETS, t);
}
int tcsetattr(int fd, int optional_actions, const struct termios* t) {
  switch (optional_actions) {
    case TCSANOW:
      return ioctl(fd, TCSETS, t);
    case TCSADRAIN:
      return ioctl(fd, TCSETSW, t);
    case TCSAFLUSH:
      return ioctl(fd, TCSETSF, t);
  }
  errno = EINVAL;
  return -1;
}
