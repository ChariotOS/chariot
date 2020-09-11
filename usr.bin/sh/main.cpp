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

#include <ck/vec.h>
#include <ck/string.h>



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

int parseline(const char *, char **argv);

auto read_line(int fd, char *prompt) {
  printf("%s", prompt);
  fflush(stdout);
  char *buf = (char*)malloc(4096);
  fgets(buf, 4096, stdin);
  buf[strlen(buf) - 1] = '\0';

	ck::string s = buf;
	free(buf);
  return s;
}


int run_line(ck::string line) {

  int err = 0;

	ck::vec<ck::string> parts = line.split(' ', false);
	ck::vec<const char *> args;

	if (parts.size() == 0) {
		return -1;
	}

	// convert it to a format that is usable by execvpe
	for (auto &p : parts) args.push(p.get());
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


  pid_t pid = fork();
  if (pid == 0) {
    execvpe(args[0], (char *const *)args.data(), environ);
		printf("execvpe returned errno '%s'\n", strerror(errno));
    exit(-1);
  }

  int stat = 0;
  waitpid(pid, &stat, 0);

  int exit_code = WEXITSTATUS(stat);
  if (exit_code != 0) {
    fprintf(stderr, "%s: exited with code %d\n", args[0], exit_code);
  }


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

  char prompt[256];
  char uname[32];
  char *cwd[255];
  char hostname[50];

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

    snprintf(prompt, 256, "[%s@%s %s]%c ", uname, hostname, disp_cwd,
             uid == 0 ? '#' : '$');

		ck::string line = read_line(0, prompt);
		if (line.len() == 0) continue;
    run_line(line);

  }

  return 0;
}

