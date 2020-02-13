#pragma once

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#ifdef __cplusplus
extern "C" {
#endif

#define __NEED_size_t
#define __NEED_off_t
#include <bits/alltypes.h>

#include <chariot/mmap_flags.h>


void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset);
int munmap(void *addr, size_t length);
int mrename(void *addr, char *name);

#ifdef __cplusplus
}
#endif

#endif
