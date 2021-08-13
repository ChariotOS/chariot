#pragma once

#include <asm.h>


void *ext4_user_malloc(unsigned long size);
void *ext4_user_calloc(unsigned long count, unsigned long size);
void *ext4_user_realloc(void *ptr, unsigned long size);
void ext4_user_free(void *ptr);