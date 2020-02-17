#pragma once

#ifndef __DIRENT__H
#define __DIRENT__H

#include <chariot/dirent.h>

typedef struct __dirstream DIR;

int closedir(DIR *);
DIR *fdopendir(int);
DIR *opendir(const char *);
struct dirent *readdir(DIR *);
/*
int readdir_r(DIR *__restrict, struct dirent *__restrict,
              struct dirent **__restrict);
              */
void rewinddir(DIR *);
int dirfd(DIR *);

#endif
