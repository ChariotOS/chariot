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

#define ENV_PATH "/cfg/environ"

extern char **environ;


static void handler(int i) { printf("=====================\nsignal handler got %d\n=====================\n", i); }


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



int futex(int *uaddr, int futex_op, int val, uint32_t timeout, int *uaddr2, int val3) {
  int r = 0;
  do {
    r = errno_wrap(sysbind_futex(uaddr, futex_op, val, 0, uaddr2, val3));
  } while (errno == EINTR);
  return r;
}


// Waits for the futex at futex_addr to have the value val, ignoring spurious
// wakeups. This function only returns when the condition is fulfilled; the only
// other way out is aborting with an error.
void wait_on_futex_value(int *futex_addr, int val) {
  while (1) {
    int futex_rc = futex(futex_addr, FUTEX_WAIT, val, NULL, NULL, 0);
    if (futex_rc == -1) {
      if (errno != EAGAIN) {
        printf("error: %s\n", strerror(errno));
        while (1) {
        }
        exit(1);
      }
    } else if (futex_rc == 0) {
      if (*futex_addr == val) {
        // This is a real wakeup.
        return;
      }
    } else {
      abort();
    }
  }
}

// A blocking wrapper for waking a futex. Only returns when a waiter has been
// woken up.
void wake_futex_blocking(int *futex_addr) {
  while (1) {
    int futex_rc = futex(futex_addr, FUTEX_WAKE, 1, NULL, NULL, 0);
    if (futex_rc == -1) {
      perror("futex wake");
      exit(1);
    } else if (futex_rc > 0) {
      return;
    }
  }
}


void futex_test(void) {
  int test = 0;


  auto thd = ck::thread([&]() {
    int *shared_data = &test;
    printf("child waiting for A\n");
    wait_on_futex_value(shared_data, 0xA);

    printf("child writing B\n");
    // Write 0xB to the shared data and wake up parent.
    *shared_data = 0xB;
    wake_futex_blocking(shared_data);
  });

  printf("parent writing A\n");
  /* Write 0xA to the shared data and wake up child. */
  test = 0xA;
  wake_futex_blocking(&test);

  printf("parent waiting for B\n");
  wait_on_futex_value(&test, 0xB);
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

int main(int argc, char **argv) {
  // open up file descriptor 1, 2, and 3
  for (int i = 0; i < 3; i++) close(i);
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

  if (getpid() != 1) {
    fprintf(stderr, "init: must be run as pid 1\n");
    return -1;
  }

  printf("[init] hello, friend\n");


  environ = read_default_environ();


  ck::vec<int> things;
  for (int i = 0; i < 15; i++) things.push(i);
  print_vector(things);
  process_vector(things, 4, [](int &i) { i = 0; });
  print_vector(things);

  // while (1) {}

#if 0
  for (int i = 0; i < 10; i++) {
    printf("================\n");
    futex_test();
  }
#endif


  FILE *loadorder = fopen("/cfg/srv/loadorder", "r");

  if (loadorder == NULL) {
    fprintf(stderr, "[init] loadorder file not found. No services shall be managed\n");
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
            pid_t pid = fork();

            if (pid == 0) {
              // child
              const char *args[] = {exec, NULL};
              execve(exec, args, (const char **)environ);
              exit(-1);
            }

            printf("[init] %s spawned on pid %d\n", name, pid);
          } else {
            printf("[init] WARN: service %s has no service.exec field\n", buf);
          }

          ini_free(i);
        }
      }
    }

    fclose(loadorder);
  }

  while (1) {
    pid_t reaped = waitpid(-1, NULL, 0);
    printf("[init] reaped pid %d\n", reaped);
  }
}

