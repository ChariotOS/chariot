#ifndef _SETJMP_H
#define _SETJMP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long __jmp_buf[8];

typedef struct __jmp_buf_tag {
  __jmp_buf __jb;
  unsigned long __fl;
  unsigned long __ss[128 / sizeof(long)];
} jmp_buf[1];

#if defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
    defined(_GNU_SOURCE) || defined(_BSD_SOURCE)
typedef jmp_buf sigjmp_buf;
int sigsetjmp(sigjmp_buf, int);
void siglongjmp(sigjmp_buf, int);
#endif

int _setjmp(jmp_buf);
void _longjmp(jmp_buf, int);

int setjmp(jmp_buf);
void longjmp(jmp_buf, int);


#ifdef __cplusplus
}
#endif

#endif
