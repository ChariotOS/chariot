#include <chariot.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sys/syscall.h>
#include <chariot/dirent.h>


#define ENV_PATH "/cfg/environ"


// read the initial environ from /etc/environ
// credit: github.com/The0x539
char **read_default_environ(void) {
  struct stat st;

  if (lstat(ENV_PATH, &st) != 0) {
    printf("[init] WARNING: no /etc/environ found\n");
    while (1) {
    }
    return NULL;
  }

  char *buf = malloc(st.st_size + 1);
  FILE *fp = fopen(ENV_PATH, "r");
  if (!fp) {
    free(buf);
    return NULL;
  }

  fread(buf, st.st_size, 1, fp);
  fclose(fp);

  size_t len = st.st_size;

  if (!buf) {
    return NULL;
  }
  size_t nvars = 0;
  for (int i = 0; i < len; i++) {
    if ((i == 0 || buf[i - 1] == '\n') && (buf[i] != '\n' && buf[i] != '#')) {
      nvars++;
    }
  }
  size_t idx = 0;
  char **env = malloc(nvars * sizeof(char *));
  for (int i = 0; i < len; i++) {
    if ((i == 0 || buf[i - 1] == '\0') && (buf[i] != '\n' && buf[i] != '#')) {
      env[idx++] = &buf[i];
    }
    if (buf[i] == '\n') {
      buf[i] = '\0';
    }
  }
  // *n = nvars;
  return env;
}

int main(int argc, char **argv) {


  if (getpid() != 1) {
    fprintf(stderr, "init: must be run as pid 1\n");
    return -1;
  }
  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i + 1);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);

  printf("[init] hello, friend\n");

  char *shell = "/bin/sh";
  char *sh_argv[] = {shell, NULL};

  char **envp = read_default_environ();

  while (1) {
    pid_t sh_pid = spawn();
    if (startpidve(sh_pid, shell, sh_argv, envp) != 0) {
      printf("failed to spawn shell\n");
      return -1;
    }

    while (1) {
      pid_t reaped = waitpid(-1, NULL, 0);

      printf("[init] reaped pid %d\n", reaped);

      if (reaped == sh_pid) {
        printf("[init] sh died\n");
        break;
      }
    }
  }
}

