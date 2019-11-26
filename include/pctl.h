#pragma once


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

