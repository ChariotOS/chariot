#pragma once

#ifndef _CHARIOT_H
#define _CHARIOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/pctl.h>

// yield system call
int yield(void);

// TODO: pid_t and all that
int spawn();

// process control system call. All the other system calls like "spawn",
// "cmdpidve" and all that are just thin wrappers around all this :)
int pctl(int pid, int req, ...);

int cmdpidve(int pid, char *const path, char *const argv[], char *const envp[]);
int cmdpidv(int pid, char *const path, char *const argv[]);

#ifdef __cplusplus
}
#endif

#endif
