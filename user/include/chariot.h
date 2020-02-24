#pragma once

#ifndef _CHARIOT_H
#define _CHARIOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/pctl.h>

#define __NEED_pid_t
#include <bits/alltypes.h>

// yield system call
int yield(void);

pid_t spawn();
pid_t despawn(int pid);


int pctl(int pid, int req, ...);


int startpidve(int pid, char *const path, char *const argv[], char *const envp[]);
int startpidvpe(int pid, char *const path, char *const argv[], char *const envp[]);
// int cmdpidv(int pid, char *const path, char *const argv[]);


#ifdef __cplusplus
}
#endif

#endif
