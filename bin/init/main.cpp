#include <chariot.h>
#include <chariot/dirent.h>
#include <chariot/futex.h>
#include <ck/eventloop.h>
#include <ck/socket.h>
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

#include "ini.h"

#define ENV_PATH "/cfg/environ"

extern char **environ;




template <typename T, typename Fn /* T& -> void */>
void process_array(T *data, size_t tasks, int nthreads, Fn cb) {
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
    j.func = &cb;
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
void process_vector(ck::vec<T> &vec, int nthreads, Fn cb) {
  auto tasks = vec.size();
  T *data = vec.data();
  process_array(data, tasks, nthreads, cb);
}


void print_vector(ck::vec<int> &vec) {
  printf("{ ");
  for (int x : vec)
    printf("%4d ", x);
  printf("}\n");
}


pid_t spawn(const char *command) {
  int pid = fork();
  if (pid == 0) {
    // they get their own pgid
    setpgid(0, 0);
    const char *args[] = {"/bin/sh", "-c", (char *)command, NULL};
    // debug_hexdump(args, sizeof(args));
    execve("/bin/sh", args, (const char **)environ);
    exit(-1);
  }
  return pid;
}


static volatile int got_signal = 0;
static void handler(int i) {
  //
  // printf("[pid=%d] signal handler got %d\n", getpid(), i);
  got_signal = 1;
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




struct init_job {};


static void sigchld_handler(int) {
  while (1) {
    int status = 0;
    /* Reap everyone available to be reaped */
    pid_t pid = waitpid(-1, &status, WNOWAIT);
    if (pid < 0) break;


    if (WIFSIGNALED(status)) {
      printf("[init] process %d terminated by signal %d\n", pid, WTERMSIG(status));
    } else {
      printf("[init] process %d exited with code %d\n", pid, WEXITSTATUS(status));
    }
  }
}


void thingy(int depth) {
  if (depth == 0) return;

  unsigned long start = sysbind_gettime_microsecond();
  int pid = fork();
  if (pid == 0) {
    exit(0);
  }
  thingy(depth - 1);
  // usleep(1000);
  // printf("pid %4d\n", pid);
  waitpid(pid, NULL, 0);

  unsigned long end = sysbind_gettime_microsecond();
  printf("pid %6d wait took %6dus\n", pid, (end - start));
}

int main(int argc, char **argv) {
  int parent_pid = getpid();
  /* Mount devfs, tmpfs */
  system("mount none /dev devfs");
  system("mount none /tmp tmpfs");
  // system("mount none /tmp procfs");

  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++)
    close(i);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);
  open("/dev/console", O_RDWR);

  sigset_t set;
  sigemptyset(&set);
  for (int i = 0; i < 32; i++) {
    sigaddset(&set, i);
    signal(i, handler);
  }
  sigprocmask(SIG_SETMASK, &set, NULL);

  printf("[init] hello, friend\n");
  system("cat /cfg/motd");

  /* Handle SIGCHLD in the  */
  signal(SIGCHLD, sigchld_handler);

  while (0) {
    thingy(10);
  }

  if (getpid() != 1) {
    fprintf(stderr, "init: must be run as pid 1\n");
    return -1;
  }

  environ = read_default_environ();


  ck::directory d;
  d.open("/cfg/init");


  for (auto &file : d.all_files()) {
    printf("file: %s\n", file.get());
  }




  ck::eventloop ev;

  ck::ipcsocket server;

#ifndef CONFIG_SIMPLE_INIT
  spawn("lumen-server");
#endif

  spawn("/bin/sh");

  ev.start();

  while (1) {
    sysbind_sigwait();
    printf("got a signal!\n");
  }
}
