#include <ck/eventloop.h>
#include <ck/io.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/sysbind.h>
#include <sys/wait.h>

static char ptsname_buf[32];
char* ptsname(int fd) {
  int id = ioctl(fd, PTMX_GETPTSID);
  if (id < 0) return NULL;

  snprintf(ptsname_buf, 32, "/dev/vtty%d", id);
  return ptsname_buf;
}


int main() {
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
