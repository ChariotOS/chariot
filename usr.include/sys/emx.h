#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/emx.h>
#include <time.h>

// return an event manager file
int emx_create(void);


// add a file descriptor to the emx object
int emx_set(int emx, int fd, void *key, int events);

// remove an entry from the emx object based on the key
int emx_rem(int emx, void *key);


// wait for some events, returning the key and setting events if not NULL
void *emx_wait(int emx, int *events);
void *emx_wait_timeout(int emx, int *events, time_t timeout);


#ifdef __cplusplus
}
#endif
