#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

void do_stat(char *path) {
  struct stat st;

  if (lstat(path, &st) != 0) {
    fprintf(stderr, "stat: failed to open file '%s'\n", path);
    exit(EXIT_FAILURE);
  }

#define F(name, thing, fmt) printf(name ": %-10" fmt, thing)
  F("- File", path, "s");
  puts("\n");

  F("  Size", st.st_size, "d");
  F("Blocks", st.st_blocks, "d");
  F(" Links", st.st_nlink, "ld");
  puts("\n");

  char mode[12];

  int m = st.st_mode;
  snprintf(mode, 12, "%c%c%c%c%c%c%c%c%c", m & S_IRUSR ? 'r' : '-',
           m & S_IWUSR ? 'w' : '-', m & S_IXUSR ? 'x' : '-',

           m & S_IRGRP ? 'r' : '-', m & S_IWGRP ? 'w' : '-',
           m & S_IXGRP ? 'x' : '-',

           m & S_IROTH ? 'r' : '-', m & S_IWOTH ? 'w' : '-',
           m & S_IXOTH ? 'x' : '-');
  F("  Mode", mode, "s");
  F("   UID", st.st_uid, "ld");
  F("   GID", st.st_gid, "ld");
  puts("\n");
}

int main(int argc, char **argv) {
  argc -= 1;
  argv += 1;

  if (argc == 0) {
    fprintf(stderr, "stat: provided no paths\n");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < argc; i++) {
    do_stat(argv[i]);
  }

  return 0;
}
