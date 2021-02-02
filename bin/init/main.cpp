#include <chariot.h>
#include <chariot/dirent.h>
#include <chariot/futex.h>
#include <ck/thread.h>
#include <ck/vec.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include "ini.h"

#include <ck/command.h>
#include <ck/dir.h>
#include <ck/io.h>
#include <ck/re.h>
#include <ck/string.h>

#include <arpa/inet.h>
#include <ck/futex.h>
#include <sys/socket.h>

#define ENV_PATH "/cfg/environ"

extern char **environ;



void tcp_test(void) {
  /* Open a UDP socket */

  // ip: 198.37.25.78


  bool worked = false;
  while (!worked) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8080);
    sysbind_dnslookup("fenrir.nickw.io", &servaddr.sin_addr.s_addr);

    int res = connect(sock, (const struct sockaddr *)&servaddr, (int)sizeof(servaddr));
    if (res == 0) {
      const char *msg = "hello world\n";
      send(sock, msg, strlen(msg), 0);
      worked = true;
    }
    close(sock);
  }
}


static void handler(int i) {
  // printf("=====================\nsignal handler got %d\n=====================\n", i);
}


// read the initial environ from /etc/environ
// credit: github.com/The0x539
char **read_default_environ(void) {
  struct stat st;

  if (lstat(ENV_PATH, &st) != 0) {
    printf("[init] WARNING: no /cfg/environ found\n");
    while (1) {
    }
    return NULL;
  }

  auto *buf = (char *)malloc(st.st_size + 1);
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
  for (size_t i = 0; i < len; i++) {
    if ((i == 0 || buf[i - 1] == '\n') && (buf[i] != '\n' && buf[i] != '#')) {
      nvars++;
    }
  }
  size_t idx = 0;
  char **env = (char **)malloc(nvars * sizeof(char *));
  for (size_t i = 0; i < len; i++) {
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




void futex_test(void) {
  ck::futex ft;

  printf("=======================================\n");

  auto thd = ck::thread([&]() {
    printf("[C] waiting A\n");
    ft.wait_on(0xA);
    printf("[C] waiting B\n");
    ft.set(0xB);
  });

  /* Write 0xA to the shared data and wake up child. */
  printf("[P] writing A\n");
  ft.set(0xA);
  printf("[P] waiting A\n");
  ft.wait_on(0xB);
}


template <typename T, typename Fn /* T& -> void */>
void process_array(T *data, size_t tasks, int nthreads, Fn fn) {
  /* Sanity :^) */
  if (nthreads < 1) nthreads = 1;

  /* A job argument is passed to a thread */
  struct job {
    int id;
    T *data;
    int count;
    Fn *func;
    pthread_t pthd;
  };
  ck::vec<job> jobs;

  int max_per_thread = (tasks + nthreads - 1) / nthreads;
  int current_start = 0;


  /* Build a plan for the threads */
  for (int i = 0; i < nthreads; i++) {
    struct job j;
    j.id = i;
    j.func = &fn;
    j.data = data + current_start;
    j.count = max_per_thread;
    int nleft = tasks - (current_start + max_per_thread);
    if (nleft < 0) j.count -= -nleft;
    current_start += max_per_thread;
    jobs.push(j);
  }


  /* Execute the plan */
  for (auto &j : jobs) {
    if (j.id == 0) continue;
    pthread_create(
        &j.pthd, NULL,
        [](void *vj) -> void * {
          struct job *j = (struct job *)vj;
          for (int i = 0; i < j->count; i++) {
            (*j->func)(j->data[i]);
          }
          return NULL;
        },
        (void *)&j);
  }


  /* evaluate the first job on the calling thread (to save resources) */
  {
    auto &j = jobs[0];
    for (int i = 0; i < j.count; i++) {
      (*j.func)(j.data[i]);
    }
  }


  for (auto &j : jobs) {
    if (j.id != 0) pthread_join(j.pthd, NULL);
  }
}




template <typename T, typename Fn /* T& -> void */>
void process_vector(ck::vec<T> &vec, int nthreads, Fn fn) {
  auto tasks = vec.size();
  T *data = vec.data();
  process_array(data, tasks, nthreads, fn);
}


void print_vector(ck::vec<int> &vec) {
  printf("{ ");
  for (int x : vec) printf("%4d ", x);
  printf("}\n");
}


pid_t spawn(const char *command) {
  int pid = fork();
  if (pid == 0) {
    const char *args[] = {"/bin/sh", "-c", (char *)command, NULL};

    // debug_hexdump(args, sizeof(args));
    execve("/bin/sh", args, (const char **)environ);
    exit(-1);
  }
  return pid;
}

int main(int argc, char **argv) {
  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  /*
sigset_t set;
sigemptyset(&set);
for (int i = 0; i < 32; i++) {
sigaddset(&set, i);
signal(i, handler);
}
sigprocmask(SIG_SETMASK, &set, NULL);
  */



	printf("init pid: %d\n", getpid());
  if (getpid() != 1) {
    fprintf(stderr, "init: must be run as pid 1\n");
    return -1;
  }

  printf("[init] hello, friend\n");

  environ = read_default_environ();

#ifndef CONFIG_SIMPLE_INIT

  spawn("lumen-server");

#endif

	/*
	int count = 0;
  while (1) {

    int pid = fork();
    if (pid == 0) {
			usleep(1000 * 1000);
			// printf("fork: %d %d\n", getpid(), gettid());
      exit(-1);
    }
    int stat;
    waitpid(pid, &stat, 0);
		printf("%d\n", count++);
  }
	*/

  spawn("/bin/sh");

  while (1) {
    pid_t reaped = waitpid(-1, NULL, 0);
    printf("[init] reaped pid %d\n", reaped);
  }
}

