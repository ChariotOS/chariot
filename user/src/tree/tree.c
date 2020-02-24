#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

long nfiles = 0;
long ndirs = 0;
int use_colors = 1;
int level = -1;

#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"

#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"

static void set_color(char *c) {
  if (use_colors) {
    printf("%s", c);
  }
}

int print_filename(const char *name, int mode) {
  char end = '\0';
  char *name_color = C_RESET;

  if (S_ISDIR(mode)) {
    end = '/';
    name_color = C_MAGENTA;
  } else {
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
      end = '*';
      name_color = C_GREEN;
    }
  }
  set_color(name_color);
  puts(name);
  set_color(C_RESET);
  printf("%c", end);

  return 1 + strlen(name);
}

static int display_info(const char *fpath, const struct stat *sb, int tflag,
                        struct FTW *ftwbuf) {
  const char *name = fpath + ftwbuf->base;

  if (tflag & FTW_D) {
    ndirs++;
  } else {
    nfiles++;
  }

  for (int i = 0; i < ftwbuf->level; i++) puts("│   ");
  puts("├── ");

  print_filename(name, sb->st_mode);
  puts("\n");

  return 0;  // To tell nftw() to continue
}

int main(int argc, char *argv[]) {
  int ftw_flags = 0;

  char ch;

  const char *flags = "L:";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'L':
        level = atoi(optarg);
        break;

      case '?':
        puts("optarg: invalid option\n");
        return -1;
    }
  }
  argc -= optind;
  argv += optind;

  // ftw_flags |= FTW_DEPTH;

  if (nftw((argc < 2) ? "." : argv[1], display_info, 20, ftw_flags) == -1) {
    // perror("nftw");
    exit(EXIT_FAILURE);
  }

  puts("\n");
  printf("%ld directories, %ld files\n", ndirs, nfiles);
  exit(EXIT_SUCCESS);
  return 0;
}
