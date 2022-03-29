#include "sys/wait.h"
#define _CHARIOT_SRC
#include <chariot.h>
#include <chariot/ucontext.h>
#include <ck/command.h>
#include <ck/func.h>
#include <ck/io.h>
#include <ck/map.h>
#include <ck/parser.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <termios.h>
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

void reset_pgid() {
  pid_t pgid = getpgid(0);

  tcsetpgrp(0, pgid);
}


pid_t shell_fork() {
  pid_t pid = fork();

  if (pid == 0) {
    // create a new group for the process
    int res = setpgid(0, 0);
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTSTP);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
  }
  return pid;
}

struct readline_history_entry {
  char *value;

  // this is represented as a singly linked list, so it's not super fast to walk
  // backwards (down arrow)
  struct readline_history_entry *next;
};

struct readline_context {
  struct readline_history_entry *history;
};

auto read_line(int fd, char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buf = (char *)malloc(4096);
  memset(buf, 0, 4096);
  fgets(buf, 4096, stdin);
  buf[strlen(buf) - 1] = '\0';

  ck::string s = (const char *)buf;

  free(buf);
  return s;
}


#define RUN_NOFORK 1



#define TOK_ARG 1
#define TOK_PIPE 2

struct ShellLexer : public ck::lexer {
  virtual ~ShellLexer(void) {}

  ck::token lex(void) override {
		
		return tok(TOK_ERR, "");
	}
};


void parse_line(ck::string_view v) {
	return;
	printf("parse '%s'\n", v.get());
}

pid_t fg_pid = -1;

int run_line(ck::string line, int flags = 0) {
	parse_line(line);
  int err = 0;
  ck::vec<ck::string> parts = line.split(' ', false);
  ck::vec<const char *> args;

  if (parts.size() == 0) {
    return -1;
  }

  // convert it to a format that is usable by execvpe
  for (auto &p : parts)
    args.push(p.get());
  args.push(nullptr);

  if (parts[0] == "cd") {
    const char *path = args[1];
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
    return err;
  }

  if (parts[0] == "exit") {
    exit(0);
  }



  pid_t pid = 0;
  if ((flags & RUN_NOFORK) == 0) {
    pid = fork();
  }

  if (pid == 0) {
    setpgid(0, 0);  // TODO: is this right?

    execvpe(args[0], args.data(), (const char **)environ);
    int err = errno;  // grab errno now (curse you globals)

    const char *serr = strerror(err);
    if (errno == ENOENT) {
      serr = "command not found";
    }
    printf("%s: \x1b[31m%s\x1b[0m\n", args[0], serr);
    exit(EXIT_FAILURE);
    exit(-1);
  }

  fg_pid = pid;

  int res = 0;
  /* wait for the subproc */
  do
    waitpid(pid, &res, 0);
  while (errno == EINTR);

  fg_pid = -1;

  if (WIFSIGNALED(res)) {
    printf("%s: \x1b[31mterminated with signal %d\x1b[0m\n", args[0], WTERMSIG(res));
  } else if (WIFEXITED(res) && WEXITSTATUS(res) != 0) {
    printf("%s: \x1b[31mexited with code %d\x1b[0m\n", args[0], WEXITSTATUS(res));
  }


  return err;
}



char hostname[256];
char prompt[256];
char uname[128];
char cwd[255];

void sigint_handler(int sig, void *, void *uc) {
#if 0
  // printf("SIGINT!\n");
  auto *ctx = (struct ucontext *)uc;
#ifdef CONFIG_X86
	printf("SIGINT from rip=%p\n", ctx->rip);
	ctx->resv3 = 0;
	ctx->resv4 = 0;
#endif
#endif

  if (fg_pid != -1) {
    kill(-fg_pid, sig);
  }
}



int main(int argc, char **argv, char **envp) {
  int ch;
  const char *flags = "c:";
  while ((ch = getopt(argc, argv, flags)) != -1) {
    switch (ch) {
      case 'c': {
        run_line(optarg, RUN_NOFORK);
        exit(EXIT_SUCCESS);
        break;
      }

      case '?':
        puts("sh: invalid option\n");
        return -1;
    }
  }


  signal(SIGINT, (void (*)(int))sigint_handler);

  /* TODO: is this right? */
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  /* TODO: isatty() :) */
  reset_pgid();

  /* Read the hostname */
  int hn = open("/cfg/hostname", O_RDONLY);
  int n = read(hn, (void *)hostname, 256);
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

	parse_line("ls -la");
	parse_line("cat /cfg/motd | hex");
	// sys::shutdown();

  struct termios tios;
  while (1) {
    tcgetattr(0, &tios);
    syscall(SYS_getcwd, cwd, 255);
    setenv("CWD", (const char *)cwd, 1);


    const char *disp_cwd = (const char *)cwd;
    if (strcmp((const char *)cwd, pwd->pw_dir) == 0) {
      disp_cwd = "~";
    }

    snprintf(prompt, 256, "[\x1b[33m%s\x1b[0m@\x1b[34m%s \x1b[35m%s\x1b[0m]%c ", uname, hostname, disp_cwd, uid == 0 ? '#' : '$');

    ck::string line = read_line(0, prompt);
    if (line.len() == 0) continue;

    run_line(line);
    tcsetattr(0, TCSANOW, &tios);
    reset_pgid();
  }

  return 0;
}
