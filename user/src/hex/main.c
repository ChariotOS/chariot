#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int human_offsets = 0;
int use_colors = 0;

#define C_RED 31
#define C_GREEN 32
#define C_YELLOW 33
#define C_BLUE 34
#define C_MAGENTA 35
#define C_CYAN 36

#define C_RESET 0
#define C_GRAY 90

static int current_color = 0;
static void set_color(int code) {
  if (use_colors) {
    if (code != current_color) {
      printf("\x1b[%dm", code);
      current_color = code;
    }
  }
}

static int human_readable_size(char *_out, int bufsz, size_t s) {
  if (s >= 1 << 20) {
    size_t t = s / (1 << 20);
    return snprintf(_out, bufsz, "%d.%.03dMB", (int)t,
                    100 * (int)(s - t * (1 << 20)) / ((1 << 20) / 10));
  } else if (s >= 1 << 10) {
    size_t t = s / (1 << 10);
    return snprintf(_out, bufsz, "%d.%03dKB", (int)t,
                    100 * (int)(s - t * (1 << 10)) / ((1 << 10) / 10));
  } else {
    return snprintf(_out, bufsz, "%dB", (int)s);
  }
}



void set_color_for(char c) {
  if (!use_colors) return;

  if (c >= 'A' && c <= 'z') {
    set_color(C_YELLOW);
  } else if (c >= '!' && c <= '~') {
    set_color(C_CYAN);
  } else if (c == '\n' || c == '\r') {
    set_color(C_GREEN);
  } else if (c == '\a' || c == '\b' || c == 0x1b || c == '\f' || c == '\n' || c == '\r') {
    set_color(C_RED);
  } else {
    set_color(C_GRAY);
  }
}

void hexdump(off_t off, void *vbuf, long len) {
  unsigned char *buf = vbuf;
  int w = 16;

  for (int i = 0; i < len; i += w) {
    unsigned char *line = buf + i;
    set_color(C_RESET);

    if (human_offsets) {
      char buf[30];
      human_readable_size(buf, 30, (off + i));
      printf("%8s: ", buf);
    } else {
      printf("%.8llx: ", (off + i));
    }

    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
        printf("   ");
      } else {
        set_color_for(line[c]);
        printf("%02x ", line[c]);
      }
    }
    set_color(C_RESET);

    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
        printf(" ");
      } else {
        set_color_for(line[c]);
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    set_color(C_RESET);
    printf("|\n");
  }
}

/* the kernel uses ext2, which has a block size of 4096 */
#define BSIZE 4096

int do_hexdump(FILE *stream) {
  size_t off = 0;

  char *buf = malloc(BSIZE);
  while (1) {
    long n = fread(buf, 1, BSIZE, stream);
    if (n < 0) {
      return -1;
    }
    if (n == 0) break;
    hexdump(off, buf, n);
    off += n;
  }
  free(buf);

  return 0;
}

const char *flags = "hC";

void usage(void) {
  printf("usage: hex [%s] [FILE]\n", flags);
  exit(1);
}

int main(int argc, char **argv) {
  char ch;

  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'h':
        human_offsets = 1;
        break;

      case 'C':
        use_colors = 1;
        break;

      case '?':
        printf("hex: invalid option\n");
        usage();
        return -1;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    usage();
    return -1;
  }

  for (int i = 0; i < argc; i++) {
    FILE *stream = fopen(argv[i], "r");
    if (stream) {
      do_hexdump(stream);
      fclose(stream);
    }
  }

  return 0;
}
