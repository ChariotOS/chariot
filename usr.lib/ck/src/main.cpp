#include <stdio.h>


extern "C" {
extern int __argc;
extern char **__argv;

// the normal libc environ variable
extern char **environ;
}
