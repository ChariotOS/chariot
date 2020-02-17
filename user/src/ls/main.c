#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define BSIZE 4096

int do_ls(char *path, int flags) {
  DIR *d = opendir(path);

  if (!d) {
    return -1;
  }


  struct dirent *ent;

  while (1) {
    ent = readdir(d);
    if (ent == NULL) break;

    printf("%s\n", ent->d_name);
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
