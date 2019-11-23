#pragma once

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_off_t
#include <bits/alltypes.h>


// failure state
#define MAP_FAILED ((void *) -1)

// TODO: add the other MAP_* operators from libc
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_ANON       0x20
#define MAP_ANONYMOUS  MAP_ANON

#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4
#define PROT_GROWSDOWN 0x01000000
#define PROT_GROWSUP   0x02000000

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset);
int munmap(void *addr, size_t length);

#ifdef __cplusplus
}
#endif

#endif
