#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define BSIZE 4096

#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"

#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"
/* this is ugly */
void print_st_mode(int m, int c) {
#define P_MOD(f, ch, col)                   \
  if (c) printf("%s", m &f ? col : C_GRAY); \
  printf("%c", m &f ? ch : '-');

  P_MOD(S_IROTH, 'r', C_YELLOW);
  P_MOD(S_IWOTH, 'w', C_RED);
  P_MOD(S_IXOTH, 'x', C_GREEN);
}

void do_ls_pretty(struct dirent *ent, struct stat *st) {
  int p_color = 1;

  /* print out the mode in a human-readable format */
  int m = st->st_mode;

  char type = '.';
  if (S_ISDIR(m)) type = 'd';
  if (S_ISBLK(m)) type = 'b';
  if (S_ISCHR(m)) type = 'c';

  if (p_color) {
    if (type != '.') {
      printf(C_CYAN);
    } else {
      printf(C_GRAY);
    }
  }
  printf("%c", type);
  print_st_mode(m >> 6, p_color);
  print_st_mode(m >> 3, p_color);
  print_st_mode(m >> 0, p_color);

  // printf(" %5o", m & 0777);

  /*
  printf(" %8u", st->st_size);
  printf(C_YELLOW);
  printf(" %4ld %4ld", st->st_uid, st->st_gid);
  printf(C_RESET);
  */

  // printf(" %03lo", st.st_mode);
  // printf(" %d", st.st_size);
  char end = '\0';
  char *color = C_RESET;
  char *rst = C_RESET;

  if (S_ISDIR(m)) {
    end = '/';
    color = "\x1b[35m";
  } else {
    if (m & (S_IXUSR | S_IXGRP | S_IXOTH)) {
      end = '*';
      color = C_GREEN;
    }
  }

  if (!p_color) {
    color = "";
    rst = "";
  }
  printf("  %s%s%s%c\n", color, ent->d_name, rst, end);
}

int do_ls(char *path, int flags) {
  DIR *d = opendir(path);

  if (!d) {
    return -1;
  }

  int l = strlen(path);

  struct dirent *ent;

  while (1) {
    ent = readdir(d);
    if (ent == NULL) break;

    // ignore dotfiles. TODO: add a flag (-a)
    if (ent->d_name[0] == '.') continue;

    int k = strlen(ent->d_name);

    // TODO: VLA is bad :^/
    char b[l + k + 1];

    memcpy(b, path, l);
    b[l] = '/';
    memcpy(b + l + 1, ent->d_name, k + 1);

    struct stat st;
    if (lstat(b, &st) != 0) {
      continue;
    }

    do_ls_pretty(ent, &st);
  }

  closedir(d);

  return 0;
}

int main(int argc, char **argv) {
  char *path = ".";

  // TODO: getopt
  if (argc > 1) {
    path = argv[1];
  }

  int res = do_ls(path, 0);
  if (res != 0) {
    printf("ls: failed to open '%s'\n", path);
  }

  return 0;
}
