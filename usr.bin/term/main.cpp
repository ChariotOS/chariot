#include <ck/eventloop.h>
#include <ck/io.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/sysbind.h>
#include <sys/wait.h>
#include <unistd.h>


extern const char **environ;


int main() {
  const char *cmd[] = {"/bin/echo", "hello, world", NULL};
  int ret = sysbind_execve(cmd[0], cmd, environ);
  perror("exec");
  exit(ret);

  int ptmxfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

  ck::eventloop ev;
  int pid = sysbind_fork();
  if (pid == 0) {
    ck::file pts;
    pts.open(ptsname(ptmxfd), "r+");

    while (1) {
      char data[32];
      auto n = pts.read(data, 32);
      if (n < 0) continue;
      ck::hexdump(data, n);
    }

    ev.start();
  } else {
    ck::file ptmx(ptmxfd);

    // send input
    ck::in.on_read(fn() {
      int c = getchar();
      if (c == EOF) return;
      ptmx.write(&c, 1);
    });


    // echo
    ptmx.on_read(fn() {
      char buf[512];
      auto n = ptmx.read(buf, 512);
      if (n < 0) {
        perror("ptmx read");
        return;
      }
      ck::out.write(buf, n);
    });

    ev.start();
  }



  return 0;
}
