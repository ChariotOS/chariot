#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>

char *buf = NULL;

long words = 0;
long chars = 0;
long lines = 0;
char *filename = 0;
long follow = 0;

int pw = 0;
int pc = 0;
int pl = 0;

void print_state(void) {
  if (pl) printf("%4ld ", lines);
  if (pw) printf("%4ld ", words);
  if (pc) printf("%5ld ", chars);
  printf("%s", filename);
}

void do_wc(FILE *stream) {
  int in_space = 0;
  while (!feof(stream)) {
    int k = fread(buf, 1, 4096, stream);
    if (k == 0) break;
    chars += k;

    for (int i = 0; i < k; i++) {
      if (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r') {
        if (!in_space) {
          words++;
          in_space = 1;
        }
      } else {
        in_space = 0;
      }

      if (buf[i] == '\n') lines++;
    }

    if (follow) {
      puts("\r");
      print_state();
    }
  }
}

int main(int argc, char **argv) {
  long tw = 0;
  long tc = 0;
  long tl = 0;

  buf = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

  int ch;
  const char *flags = "lwcf";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'l':
        pl = 1;
        break;
      case 'w':
        pw = 1;
        break;

      case 'c':
        pc = 1;
        break;

      case 'f':
        follow = 1;
        break;

      case '?':
        puts("wc: invalid option\n");
        printf("    usage: %s [%s] [FILE...]\n", argv[0], flags);
        return -1;
    }
  }

  argc -= optind;
  argv += optind;

  if (!pc && !pl) pw = 1;

  if (argv > 0) {
    for (int i = 0; i < argc; i++) {
      FILE *stream = fopen(argv[i], "r");
      filename = argv[i];
      if (stream == NULL) {
        fprintf(stderr, "Could not open '%s'\n", argv[i]);
        exit(EXIT_FAILURE);
      }
      do_wc(stream);
      fclose(stream);
      print_state();
      puts("\n");

      tl += lines;
      tw += words;
      tc += chars;

      // reset
      lines = words = chars = 0;
    }
  } else {
    printf("TODO: stdin\n");
    filename = "stdin";
    print_state();
    puts("\n");

    tl += lines;
    tw += words;
    tc += chars;
  }

  munmap(buf, 4096);

  return 0;
}
