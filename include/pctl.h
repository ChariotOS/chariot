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
