#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define BSIZE 4096

#define C_RED "\x1b[91m"
#define C_GREEN "\x1b[92m"
#define C_YELLOW "\x1b[93m"
#define C_BLUE "\x1b[94m"
#define C_MAGENTA "\x1b[95m"
#define C_CYAN "\x1b[96m"

#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"

struct lsfile {
  char *name;
  struct stat st;
};

static int use_colors = 0;
static int term_width = 80;  // default

static void set_color(char *c) {
  if (use_colors) {
    printf("%s", c);
  }
}

/* this is ugly */
void print_st_mode(int m) {
#define P_MOD(f, ch, col)         \
  set_color(m &f ? col : C_GRAY); \
  printf("%c", m &f ? ch : '-');

  P_MOD(S_IROTH, 'r', C_YELLOW);
  P_MOD(S_IWOTH, 'w', C_RED);
  P_MOD(S_IXOTH, 'x', C_GREEN);
}

static int human_readable_size(char *_out, int bufsz, size_t s) {
  if (s >= 1 << 20) {
    size_t t = s / (1 << 20);
    return snprintf(_out, bufsz, "%d.%1dM", (int)t,
                    (int)(s - t * (1 << 20)) / ((1 << 20) / 10));
  } else if (s >= 1 << 10) {
    size_t t = s / (1 << 10);
    return snprintf(_out, bufsz, "%d.%1dK", (int)t,
                    (int)(s - t * (1 << 10)) / ((1 << 10) / 10));
  } else {
    return snprintf(_out, bufsz, "%d", (int)s);
  }
}

#define FL_L (1 << 0)
#define FL_1 (1 << 1)
#define FL_a (1 << 2)
#define FL_i (1 << 3)

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




#define UCACHE_LEN 20
struct username {
  int last_used;
  int uid;
  char name[64];
};

static int next_cache_time = 2;
static struct username username_cache[UCACHE_LEN];

char *getusername(int uid) {
  struct username *u = NULL;
  int oldest_time = INT_MAX;


  // find a cache entry
  for (int i = 0; i < UCACHE_LEN; i++) {
    struct username *c = username_cache + i;

    if (c->uid == uid) {
      c->last_used = next_cache_time++;
      return c->name;
    }

    if (c->last_used < oldest_time) {
      u = c;
    }
  }

  if (u == NULL) {
    fprintf(stderr, "username is null!\n");
    exit(EXIT_FAILURE);
  }


  struct passwd *pswd = getpwuid(uid);
  memcpy(u->name, pswd->pw_name, strlen(pswd->pw_name));
  u->uid = 0;
  u->last_used = next_cache_time++;




  return u->name;
}


void print_entries_long(struct lsfile **ents, int entc, long flags) {
  int longest_username = 0;
  int longest_filesize = 0;

  int human_readable = 1;


  for (int i = 0; i < 20; i++) {
    username_cache[i].uid = -1;
    username_cache[i].last_used = 0;
  }



  for (int i = 0; i < entc; i++) {
    struct lsfile *ent = ents[i];
    char *uname = getusername(ent->st.st_uid);
    int len = strlen(uname);
    if (len > longest_username) longest_username = len;
  }

  for (int i = 0; i < entc; i++) {
    char buf[32];
    struct lsfile *ent = ents[i];
    int len = 1;
    if (!S_ISDIR(ent->st.st_mode)) {
      if (human_readable) {
        human_readable_size(buf, 32, ent->st.st_size);
      } else {
        snprintf(buf, 32, "%d", ent->st.st_size);
      }
      len = strlen(buf);
    }
    if (len > longest_filesize) longest_filesize = len;
  }

  for (int i = 0; i < entc; i++) {
    struct lsfile *ent = ents[i];
    int m = ent->st.st_mode;
    /* print out the mode in a human-readable format */
    char type = '.';
    if (S_ISDIR(m)) type = 'd';
    if (S_ISBLK(m)) type = 'b';
    if (S_ISCHR(m)) type = 'c';
    if (S_ISLNK(m)) type = 'l';

    if (type != '.') {
      set_color(C_CYAN);
    } else {
      set_color(C_GRAY);
    }
    printf("%c", type);
    print_st_mode(m >> 6);
    print_st_mode(m >> 3);
    print_st_mode(m >> 0);

    set_color(C_YELLOW);
    printf(" %*s", longest_username, getusername(ent->st.st_uid));

    /*
    printf(" %8ld", ent->st.st_mtim);
    printf(" %8ld", ent->st.st_atim);
    printf(" %8ld", ent->st.st_ctim);
    */

    // print out the filesize
    if (S_ISDIR(m)) {
      set_color(C_GRAY);

      printf(" %*s", longest_filesize, "-");
    } else {
      set_color(C_GREEN);
      set_color("\x1b[1m");
      if (human_readable) {
        char filesz_buf[32];
        human_readable_size(filesz_buf, 32, ent->st.st_size);
        printf(" %*s", longest_filesize, filesz_buf);
      } else {
        printf(" %*d", longest_filesize, ent->st.st_size);
      }
    }

    puts(" ");
    print_filename(ent->name, m);
    puts("\n");
  }
}
static inline int max(int a, int b) { return (a > b) ? a : b; }

void print_entry(struct lsfile *ent, int colwidth) {
  int n = print_filename(ent->name, ent->st.st_mode);
  for (int rem = colwidth - n; rem > 0; rem--) {
    puts(" ");
  }
}

void print_entries(struct lsfile **ents, int entc, long flags) {
  /* Determine the gridding dimensions */
  int ent_max_len = 0;
  for (int i = 0; i < entc; i++) {
    ent_max_len = max(ent_max_len, (int)strlen(ents[i]->name));
  }

  int col_ext = ent_max_len + 2;
  int cols = ((term_width - ent_max_len) / col_ext) + 1;

  if (flags & FL_1) cols = 1;

  /* Print the entries */
  for (int i = 0; i < entc;) {
    /* Columns */
    print_entry(ents[i++], ent_max_len);
    for (int j = 0; (i < entc) && (j < (cols - 1)); j++) {
      puts("  ");
      print_entry(ents[i++], ent_max_len);
    }

    puts("\n");
  }
}

int lsfile_compare(const void *av, const void *bv) {
  const struct lsfile *d1 = av;
  const struct lsfile *d2 = bv;

  int a = S_ISDIR(d1->st.st_mode);
  int b = S_ISDIR(d2->st.st_mode);

  if (a == b)
    return strcmp(d1->name, d2->name);
  else if (a < b)
    return -1;
  else if (a > b)
    return 1;
  return 0; /* impossible ? */
}

int do_ls(char *path, long flags) {
  DIR *d = opendir(path);
  if (!d) return -ENOENT;

  int l = strlen(path);
  struct dirent *ent;
  int ents = 0;

  for (ents = 0; (ent = readdir(d)) != NULL;) {
    if ((flags & FL_a) == 0 && ent->d_name[0] == '.') {
      continue;
    } else {
      ents++;
    }
  }
  rewinddir(d);

  struct lsfile **files = calloc(sizeof(struct lsfile *), ents);

  char *reified_path = NULL;
  for (int i = 0; i < ents; i++) {
    ent = readdir(d);
    if (ent == NULL) return -1;

    if ((flags & FL_a) == 0 && ent->d_name[0] == '.') {
      i--;
      continue;
    }

    files[i] = calloc(sizeof(struct lsfile), 1);
    struct lsfile *f = files[i];

    f->name = strdup(ent->d_name);
    int k = strlen(ent->d_name);
    reified_path = realloc(reified_path, l + k + 1);
    memcpy(reified_path, path, l);
    reified_path[l] = '/';
    memcpy(reified_path + l + 1, ent->d_name, k + 1);
    if (lstat(reified_path, &f->st) != 0) continue;
  }
  free(reified_path);

  closedir(d);

  qsort(files, ents, sizeof(struct lsfile *), lsfile_compare);

  for (int i = 0; i < ents; i++) {
    struct lsfile *tmp = NULL;
    if (!strcmp(files[i]->name, ".")) {
      if (i != 0) {
        tmp = files[i];
        files[i] = files[0];
        files[0] = tmp;
      }
    }

    if (!strcmp(files[i]->name, "..")) {
      if (i != 1) {
        tmp = files[i];
        files[i] = files[1];
        files[1] = tmp;
      }
    }
  }

  if (flags & FL_L) {
    print_entries_long(files, ents, flags);
  } else {
    print_entries(files, ents, flags);
  }

  // cleanup
  for (int i = 0; i < ents; i++) {
    free(files[i]->name);
    free(files[i]);
  }
  free(files);
  return 0;
}

int main(int argc, char **argv) {
  char *path = ".";

  char ch;

  long todo = 0;

  const char *flags = "1liCa";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case '1':
        todo |= FL_1;
        break;
      case 'l':
        todo |= FL_L;
        break;

      case 'i':
        todo |= FL_i;
        break;

      case 'C':
        use_colors = 1;
        break;

      case 'a':
        todo |= FL_a;
        break;

      case '?':
        puts("ls: invalid option\n");
        printf("    usage: %s [%s] [FILE]\n", argv[0], flags);
        return -1;
    }
  }

  // TODO: derive this from isatty()
  use_colors = 1;

  argc -= optind;
  argv += optind;

  if (argc >= 1) {
    path = argv[0];
    for (int i = 0; i < argc; i++) {
      int res = do_ls(argv[i], todo);
      if (res != 0) {
        printf("ls: failed to open '%s'\n", path);
      }
    }
  } else {
    int res = do_ls(".", todo);
    if (res != 0) {
      printf("ls: failed to open '%s'\n", path);
    }
  }

  return 0;
}

