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
  ck::file ptmx(open("/dev/ptmx", O_RDWR | O_CLOEXEC));

  ck::file pts;
  pts.open(ptsname(ptmx.fileno()), "r+");

	int pid = sysbind_fork();
  if (pid == 0) {
    // forked!
    int n = pts.fmt("hello, terminal\n");
		printf("n=%d\n", n);
    exit(0);
  } else {
    char buf[512];
    int n = ptmx.read(buf, 512);
    if (n >= 0) {
      ck::hexdump(buf, n);
    } else {
      perror("ptmx read");
    }
    waitpid(pid, NULL, 0);
  }



  return 0;
}
