#pragma once

#include <riscv/arch.h>

typedef rv::xsize_t __jmp_buf[14];

typedef struct {
  __jmp_buf __jb;
} jmp_buf[1];

extern "C" {
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
}
