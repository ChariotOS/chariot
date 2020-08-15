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
  int ptmxfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

  int pid = sysbind_fork();
  if (pid == 0) {

		// setup the sub proc's 
    int pty = open(ptsname(ptmxfd), O_RDWR);
    for (int i = 0; i < 3; i++) close(i);
    for (int i = 0; i < 3; i++) dup2(pty, i);

    const char *cmd[] = {"/bin/sh", NULL};
    int ret = sysbind_execve(cmd[0], cmd, environ);
    perror("exec");
    exit(ret);
  }



  ck::eventloop ev;
  ck::file ptmx(ptmxfd);

  // send input
  ck::in.on_read(fn() {
    int c = getchar();
    if (c == EOF) return;
    ptmx.write(&c, 1);
  });

  // echo
  ptmx.on_read(fn() {
    // read 512 bytes at a time
    char buf[4096];
    auto n = ptmx.read(buf, 4096);
    if (n < 0) {
      perror("ptmx read");
      return;
    }
    ck::out.write(buf, n);
  });

  ev.start();

  return 0;
}
