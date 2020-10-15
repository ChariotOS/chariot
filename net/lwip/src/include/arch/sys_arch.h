#ifndef __LWIP_SYS_H__
#define __LWIP_SYS_H__

#include <lock.h>
#include <chan.h>
#include <sem.h>


// typedef semaphore sys_sem;
// typedef spinlock sys_mutex;
typedef int sys_thread_t;
// typedef struct nk_msg_queue sys_mbox;

typedef struct{
	int locked;
	int valid;
} sys_mutex_t;

typedef struct {
	int valid;
	semaphore *sem;
} sys_sem_t;


typedef struct {
	chan<void *> *ch;
} sys_mbox_t;

#endif
