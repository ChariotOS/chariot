#define _CHARIOT_SRC
#include <chariot.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define C_RED "\x1b[31m"
#define C_GREEN "\x1b[32m"
#define C_YELLOW "\x1b[33m"
#define C_BLUE "\x1b[34m"
#define C_MAGENTA "\x1b[35m"
#define C_CYAN "\x1b[36m"
#define C_RESET "\x1b[0m"
#define C_GRAY "\x1b[90m"


size_t current_us() { return syscall(SYS_gettime_microsecond); }

extern char **environ;

#define MAX_ARGS 255

struct readline_history_entry {
  char *value;

  // this is represented as a singly linked list, so it's not super fast to walk
  // backwards (down arrow)
  struct readline_history_entry *next;
};

struct readline_context {
  struct readline_history_entry *history;
};

char *read_line(int fd, char *prompt, int *len_out, struct readline_context *);
int parseline(const char *, char **argv);

int run_line(const char *line) {
  int err = 0;
  int arg_buflen = sizeof(char *) * MAX_ARGS;
  char **args = calloc(sizeof(char *), MAX_ARGS);
  memset(args, 0, arg_buflen * sizeof(char *));

  if (strcmp(line, "exit") == 0) exit(0);
  int bg = parseline(line, args);

  if (args[0] == NULL) goto cleanup;


  if (strcmp(args[0], "fork") == 0) {
    pid_t p = syscall(SYS_fork);

    printf("p = %d\n", p);
    if (p >= 0) {
      int stat = 0;
      if (p == 0) {
      } else {
        waitpid(p, &stat, 0);
      }
    }
    goto cleanup;
  }

  if (strcmp(args[0], "cd") == 0) {
    char *path = args[1];
    if (path == NULL) {
      uid_t uid = getuid();
      struct passwd *pwd = getpwuid(uid);
      // TODO: get user $HOME and go there instead
      path = pwd->pw_dir;
    }
    int res = chdir(path);
    if (res != 0) {
      printf("cd: '%s' could not be entered\n", args[1]);
    }
    goto cleanup;
  }


  pid_t pid = fork();
  if (pid == 0) {
    execvpe(args[0], args, environ);
    exit(-1);
  }

  int stat = 0;
  if (!bg) {
    waitpid(pid, &stat, 0);
  }

  int exit_code = WEXITSTATUS(stat);
  if (exit_code != 0) {
    fprintf(stderr, "%s: exited with code %d\n", args[0], exit_code);
  }
cleanup:
  free(args);
  return err;
}

int main(int argc, char **argv, char **envp) {
  char ch;
  const char *flags = "c:";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'c':
        return run_line(optarg);
        break;

      case '?':
        puts("sh: invalid option\n");
        return -1;
    }
  }

  char prompt[64];
  char uname[32];
  char *cwd[255];
  char hostname[50];

  struct readline_context rl_ctx;
  rl_ctx.history = NULL;

  int hn = open("/cfg/hostname", O_RDONLY);

  int n = read(hn, (void *)hostname, 50);
  if (n >= 0) {
    hostname[n] = 0;
    for (int i = n; i > 0; i--) {
      if (hostname[i] == '\n') hostname[i] = 0;
    }
  }

  close(hn);


  uid_t uid = getuid();
  struct passwd *pwd = getpwuid(uid);
  strncpy(uname, pwd->pw_name, 32);


  setenv("USER", pwd->pw_name, 1);
  setenv("SHELL", pwd->pw_shell, 1);
  setenv("HOME", pwd->pw_dir, 1);

  while (1) {
    syscall(SYS_getcwd, cwd, 255);
    setenv("CWD", (const char *)cwd, 1);


    const char *disp_cwd = (const char *)cwd;
    if (strcmp((const char *)cwd, pwd->pw_dir) == 0) {
      disp_cwd = "~";
    }

    snprintf(prompt, 64, "[%s@%s %s]%c ", uname, hostname, disp_cwd,
             uid == 0 ? '#' : '$');
    // snprintf(prompt, 64, "%s %c ", disp_cwd, uid == 0 ? '#' : '$');

    int len = 0;

    char *buf = read_line(0, prompt, &len, &rl_ctx);

    len = strlen(buf);
    if (len == 0) goto cleanup;

    run_line(buf);

  cleanup:
    free(buf);
  }

  return 0;
}

struct input_info {
  // cursor index in the buffer
  int ind;
  int len;
  // size of buffer
  int max;
  char *buf;
};

void input_insert(char c, struct input_info *in) {
  // ensure there is enough space for the character insertion
  if (in->len + 1 >= in->max - 1) {
    in->max *= 2;
    in->buf = realloc(in->buf, in->max);
  }

  // insert
  // _________|__________________00000000000
  //          ^                  ^         ^
  //          ind                len       max

  // move the bytes after the cursor right by one
  memmove(in->buf + in->ind + 1, in->buf + in->ind, in->len - in->ind);
  in->buf[in->ind] = c;
  in->ind++;
  in->buf[++in->len] = '\0';
}

void input_del(struct input_info *in) {
  if (in->ind == 0) return;
  int i = in->ind;
  int l = in->len;

  // move the bytes after the cursor left by one
  memmove(in->buf + i - 1, in->buf + i, l - i);
  in->ind--;
  in->buf[--in->len] = '\0';
}

void input_display(struct input_info *in, const char *prompt) {
  // TODO: this wastes IO operations *alot*
  fprintf(stderr, "\x1b[2K\r");
  // fprintf(stderr, "%4d ", in->ind);
  fprintf(stderr, "%s", prompt);
  fprintf(stderr, "%s", in->buf);
  // CSI n C will move by 1 even when you ask for 0...
  int dist = in->len - in->ind;
  if (dist != 0) {
    if (dist > 6 /* arbitrary */) {
      for (int i = 0; i < dist; i++) fputc('\b', stderr);
    } else {
      fprintf(stderr, "\x1b[%dD", dist);
    }
  }

  fflush(stderr);
}

void handle_special(char c, struct input_info *in) {
  switch (c) {
    case 0x01:
      in->ind = 0;
      break;

    case 0x05:
      in->ind = in->len;
      break;

      // C-u to clear the input
    case 0x15:
      memset(in->buf, 0, in->len);
      in->ind = 0;
      in->len = 0;
      break;

    default:
      printf("special input: 0x%02x\n", c);
  }
}

int select_historic_input(struct input_info *in, int n,
                          struct readline_context *ctx) {
  // clear the input
  memset(in->buf, 0, in->len);
  in->ind = 0;
  in->len = 0;

  if (n > -1) {
    int i = 0;
    for (struct readline_history_entry *hist = ctx->history; hist != NULL;
         hist = hist->next) {
      if (i++ == n) {
        for (int c = 0; c < strlen(hist->value); c++) {
          input_insert(hist->value[c], in);
        }
        return 0;
      }
    }

    return -1;
  }

  return -1;
}

// the number of ANSI paramters
#define NPAR 16

char *read_line(int fd, char *prompt, int *len_out,
                struct readline_context *ctx) {
  printf("%s", prompt);
  fflush(stdout);
  char *buf = malloc(4096);
  fgets(buf, 4096, stdin);
  buf[strlen(buf) - 1] = '\0';
  *len_out = strlen(buf);
  return buf;

  struct input_info in;

  in.max = 32;
  in.len = 0;
  in.ind = 0;
  in.buf = calloc(in.max, 1);

  // ANSI escape code state machine
  int state = 0;
  static unsigned long npar, par[NPAR];
  int ques = 0;

  /*
int i = 0;
for (struct readline_history_entry *hist = ctx->history; hist != NULL;
 hist = hist->next) {
printf("%2d: %s\n", i++, hist->value);
}
  */

  int history_index = -1;


  while (1) {
    input_display(&in, prompt);
    int c = fgetc(stdin);

    if (c == -1) break;
    if (c == '\n' || c == '\r') break;

    if (in.len + 1 >= in.max - 1) {
      in.max *= 2;
      in.buf = realloc(in.buf, in.max);
    }

    switch (state) {
      // default text-entry mode
      case 0:
        if (c == 0x1b) {
          state = 1;
          break;
        } else if (c == 0x7F /* DEL */) {
          history_index = -1;
          input_del(&in);
        } else if (c >= ' ' && c <= '~') {
          history_index = -1;
          input_insert(c, &in);
        } else {
          history_index = -1;
          // special input handling
          handle_special(c, &in);
        }
        break;

      case 1:
        state = 0;
        if (c == '[') {
          state = 2;
        }
        break;

      case 2:
        for (npar = 0; npar < NPAR; npar++) par[npar] = 0;
        npar = 0;
        state = 3;
        if ((ques = (c == '?'))) {
          break;
        }

        /* parse out decimal arguments */
      case 3:
        if (c == ';' && npar < NPAR - 1) {
          npar++;
          break;
        } else if (c >= '0' && c <= '9') {
          par[npar] = 10 * par[npar] + c - '0';
          break;
        } else
          state = 4;
      /* run ansi codes */
      case 4:
        state = 0;
        switch (c) {
          // up arrow
          case 'A':

            if (0 == select_historic_input(&in, history_index + 1, ctx)) {
              history_index++;
            }
            break;

          // down arrow
          case 'B':
            if (0 == select_historic_input(&in, history_index - 1, ctx)) {
              history_index--;
            }
            break;

          case 'D':
            if (in.ind > 0) in.ind--;
            break;
          case 'C':
            if (in.ind < in.len) in.ind++;
            break;
          case '~':
            if (in.ind < in.len) {
              in.ind++;
              input_del(&in);
            }
            // right delete
            break;

          default:
            fprintf(stderr, "unknown ansi escp: \\x1b[");
            for (int i = 0; i < npar; i++) {
              fprintf(stderr, "%lu\n", par[i]);
              if (i != npar - 1) fprintf(stderr, ";");
            }
            fprintf(stderr, "%c\n", c);
            break;
        }
        break;
    }
  }
  in.buf[in.len] = '\0';  // null terminate
  if (len_out != NULL) *len_out = in.len;

  fprintf(stderr, "\n");

  // push to history
  struct readline_history_entry *ent = calloc(1, sizeof(*ent));

  int save = 1;

  // don't save if we didn't type anything
  if (strlen(in.buf) == 0) {
    save = 0;
  }

  if (ctx->history) {
    // don't save the entry if its the same as the previous
    if (strcmp(ctx->history->value, in.buf) == 0) {
      save = 0;
    }
  }

  if (save) {
    ent->value = strdup(in.buf);
    ent->next = ctx->history;
    ctx->history = ent;
  }

  return in.buf;
}

int parseline(const char *cmdline, char **argv) {
  char *buf = malloc(strlen(cmdline) + 1); /* ptr that traverses command line */
  char *delim;                             /* points to first space delimiter */
  int argc;                                /* number of args */
  int bg;                                  /* background job? */

  strcpy(buf, cmdline);
  buf[strlen(buf)] = ' ';       /* replace trailing '\n' with space */
  while (*buf && (*buf == ' ')) /* ignore leading spaces */
    buf++;

  /* Build the argv list */
  argc = 0;
  if (*buf == '\'') {
    buf++;
    delim = strchr(buf, '\'');
  } else {
    delim = strchr(buf, ' ');
  }

  while (delim) {
    argv[argc++] = buf;
    *delim = '\0';
    buf = delim + 1;
    while (*buf && (*buf == ' ')) /* ignore spaces */
      buf++;

    if (*buf == '\'') {
      buf++;
      delim = strchr(buf, '\'');
    } else {
      delim = strchr(buf, ' ');
    }
  }
  argv[argc] = NULL;

  if (argc == 0) /* ignore blank line */
    return 1;

  /* should the job run in the background? */
  if ((bg = (*argv[argc - 1] == '&')) != 0) {
    argv[--argc] = NULL;
  }
  return bg;
}
