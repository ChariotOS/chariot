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
int despawn(int pid);

int pctl(int pid, int req, ...);


int startpidve(int pid, char *const path, char *const argv[], char *const envp[]);
int startpidvpe(int pid, char *const path, char *const argv[], char *const envp[]);
// int cmdpidv(int pid, char *const path, char *const argv[]);


#ifdef __cplusplus
}
#endif

#endif
