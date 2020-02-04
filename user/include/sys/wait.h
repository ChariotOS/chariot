#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H
#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_pid_t
#define __NEED_id_t
#include <bits/alltypes.h>

// pull flags from the kernel
#include <chariot/wait_flags.h>

pid_t wait(int *);
pid_t waitpid(pid_t, int *, int);


#ifdef __cplusplus
}
#endif
#endif
