#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

struct __dirstream {
  int fd;
  int pos;
  int nents;
  // volatile int lock[1];
  struct dirent *ents;
};

static DIR *populate_dir(DIR *dir) {
  dir->ents = NULL;
  dir->pos = 0;

  struct stat st;

  if (fstat(dir->fd, &st) < 0) {
    free(dir);
    return 0;
  }
  /** TODO
  if (!S_ISDIR(st.st_mode)) {
  printf("mode=%zo\n", st.st_mode);
    // TODO: errno
    // errno = ENOTDIR;
    free(dir);
    return 0;
  }
  */

  dir->nents = syscall(SYS_dirent, dir->fd, NULL, 0, 0);
  if (dir->nents < 0) {
    free(dir);
    return NULL;
  }

  dir->ents = calloc(dir->nents, sizeof(struct dirent));
  int n = syscall(SYS_dirent, dir->fd, dir->ents, 0, dir->nents);

  if (n != dir->nents) {
    printf("populate_dir: hmm\n");
  }

  return dir;
}

DIR *opendir(const char *name) {
  int fd;
  DIR *dir;

  if ((fd = open(name, O_RDONLY | O_DIRECTORY | O_CLOEXEC)) < 0) return 0;
  if (!(dir = calloc(1, sizeof *dir))) {
    close(fd);
    return 0;
  }
  dir->fd = fd;

  return populate_dir(dir);
}

DIR *fdopendir(int fd) {
  DIR *dir;
  if (!(dir = calloc(1, sizeof *dir))) {
    return 0;
  }

  dir->fd = fd;
  return populate_dir(dir);
}


struct dirent *readdir(DIR *d) {
  if (d->pos == d->nents) return NULL;

  // TODO: lock
  return &d->ents[d->pos++];
}

int closedir(DIR *dir) {
  int ret = close(dir->fd);
  free(dir);
  return ret;
}


void rewinddir(DIR *d) {
  d->pos = 0;
}

int dirfd(DIR *d) {
  return d->fd;
}
