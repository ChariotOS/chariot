// defer to the kernel's implementation
#include "../../kernel/lib/ck.cpp"

#include <stdlib.h>
#include <string.h>
#include <ck/command.h>
#include <ck/defer.h>
#include <sys/wait.h>

extern char **environ;

ck::command::command(ck::string exe) : m_exe(exe) {}


ck::command::~command(void) {
  if (pid != -1) {
		// XXX: What should we do here?
  }
}


void ck::command::arg(ck::string arg) { m_args.push(arg); }



int ck::command::start(void) {

  pid = spawn();
  if (pid <= -1) {
    // perror("ck::command::exec spawn");
    return pid;
  }

  // build the argv list
  auto argv = new char *[m_args.size() + 2];
	defer({ delete argv; });

	argv[0] = strdup(m_exe.get());
	for (int i = 0; i < m_args.size(); i++) {
		argv[i + 1] = strdup(m_args[i].get());
	}
	argv[m_args.size() + 2] = NULL;

	// attempt to start the pid
  int start_res =
      startpidvpe(pid, (char*)argv[0], (char**)argv, environ);

  if (start_res != 0) {
    // perror("ck::command::exec startpidvpe");
    despawn(pid);
    pid = -1;
    return -1;
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
