#include <chariot.h>
#include <chariot/dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ini.h"

#define ENV_PATH "/cfg/environ"

extern char **environ;

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

struct service {
  //
};


unsigned long read_timestamp(void) {
  unsigned int lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((unsigned long)(hi) << 32);
}

int main(int argc, char **argv) {
	unsigned long tsc = read_timestamp();

  if (getpid() != 1) {
    fprintf(stderr, "init: must be run as pid 1\n");
    return -1;
  }


  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i + 1);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);


	printf("ticks to code: %zu\n", tsc);

  printf("[init] hello, friend\n");

  FILE *loadorder = fopen("/cfg/srv/loadorder", "r");

  if (loadorder == NULL) {
    fprintf(stderr,
	    "[init] loadorder file not found. No services shall be managed\n");
  } else {
    char buf[255];

    while (!feof(loadorder)) {
      if (fgets(buf, 255, loadorder) != NULL) {
	for (int i = strlen(buf); i >= 0; i--) {
	  if (buf[i] == '\n') {
	    buf[i] = '\0';
	    break;
	  }
	}
	buf[strlen(buf)] = 0;
	ini_t *i = ini_load(buf);

	if (i) {
	  const char *exec = ini_get(i, "service", "exec");

	  const char *name = ini_get(i, "service", "name");
	  if (exec != NULL) {
	    pid_t pid = spawn();
	    char *args[] = {(char *)exec, NULL};

	    // just let it go
	    int res = startpidvpe(pid, (char *)exec, args, environ);
	    int e = errno;
	    if (res == 0) {
	      printf("[init] %s spawned on pid %d\n", name, pid);
	    } else {
	      printf("[init] %s - %s\n", name, strerror(e));
	      waitpid(pid, NULL, 0);
	    }
	  } else {
	    printf("[init] WARN: service %s has no service.exec field\n", buf);
	  }

	  ini_free(i);
	}
      }
    }

    fclose(loadorder);
  }

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

