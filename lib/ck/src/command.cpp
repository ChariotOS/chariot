#include <ck/command.h>
#include <ck/defer.h>
#include <ck/io.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

ck::command::command(ck::string exe) : m_exe(exe) {
}


ck::command::~command(void) {
  if (pid != -1) {
    // XXX: What should we do here?
  }
}


void ck::command::arg(ck::string arg) {
  m_args.push(arg);
}



int ck::command::start(void) {
  if (pid != -1) return -EEXIST;

  pid = fork();
  if (pid == 0) {
    // build the argv list
    auto argv = new char *[m_args.size() + 2];
    defer({ delete[] argv; });

    argv[0] = strdup(m_exe.get());
    for (int i = 0; i < m_args.size(); i++) {
      argv[i + 1] = strdup(m_args[i].get());
    }
    argv[m_args.size() + 2] = NULL;
    execvpe((const char *)argv[0], (const char **)argv, (const char **)environ);
    exit(-1);
  }

  return pid;
}

int ck::command::exec(void) {
  int res = start();
  if (res < 0) {
    return -1;
  }
  return wait();
}

int ck::command::wait(void) {
  int stat = -1;
  if (pid != -1) {
    // We are probably losing out on some interesting information if this fails
    waitpid(pid, &stat, 0);
    pid = -1;
  }
  return stat;
}


ck::string ck::format(const ck::command &cmd) {
  ck::string s = cmd.exe();

  auto argc = cmd.argc();

  auto argv = cmd.argv();
  for (int i = 0; i < argc; i++) {
    s += " ";      // add the spacing
    s += argv[i];  // add the argument
  }

  return s;
}
