#pragma once

#define PID_SELF -2

#define PCTL_SPAWN 0
// command the process to run a binary
#define PCTL_CMD 1

struct pctl_cmd_args {
  char *path;

  int argc;
  char **argv;

  int envc;
  char **envv;
};

// signal sending and waiting.
#define PCTL_SEND_SIGNAL 2

#define PCTL_WAITPID 3

// exit the task_process
#define PCTL_EXIT_PROC 4
// exit just the task
#define PCTL_EXIT_TASK 5

struct pctl_wait_args {
  // TODO:
};

#define PCTL_CREATE_THREAD 6

/**
 * create a thread, calling the function `fn` with the stack `stack`
 */
struct pctl_create_thread_args {
  int flags;  // TODO
  long tid;   // (OUT) set by syscall

  // the argument to be passed to
  void *arg;
  // the function to be called
  int (*fn)(void *); /*  */

  void *stack;     /* pointer to lowest byte of the stack */
  long stack_size; /* size of the stack */

  // // TODO: use the tls
  // void *tls;
};
