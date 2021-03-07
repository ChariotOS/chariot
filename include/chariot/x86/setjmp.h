#pragma once

typedef unsigned long __jmp_buf[8];
typedef struct {
  __jmp_buf __jb;
  unsigned long __fl;
  unsigned long __ss[128 / sizeof(long)];
} jmp_buf[1];

extern "C" {
int setjmp(jmp_buf);
void longjmp(jmp_buf, int);
}
