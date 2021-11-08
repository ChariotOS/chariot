#pragma once
#include <ck/vec.h>
#include <ck/string.h>
#include <module.h>

// the kshell related function calls are for debugging the kernel from userspace.
// (or somewhere else, if you want I guess). They arent meant to return anything,
// but just output to the debug console (ideally)
// Imagine it like an actually command line interface, but as an IPC interface to the kernel.
// They all require root (uid=0) to run :)
namespace kshell {
  using handler = unsigned long (*)(ck::vec<ck::string> &, void *data, int dlen);
  void add(ck::string command, ck::string usage, handler h);
  unsigned long call(ck::string command, ck::vec<ck::string> args, void *data, size_t dlen);


	// run the kshell on this kernel thread. This is basically a debug
	// console in the kernel for when something breaks :)
	void run(const char *prompt = "##>");


	// if the kernel shell is currently being run on a thread.
	bool active(void);

	// feed input (from the console, most likely)
	void feed(size_t sz, char *buf);

};  // namespace kshell



// use like `ksh_def("test", "test") {}` in the
// top level to define a ksh command nice and easy.
// `args` is defined in the body of the function
#define ksh_def(cmd, usage) \
	static unsigned long _MOD_VARNAME(ksh_command)(ck::vec<ck::string> &args, void *data, int dlen); \
	module_init_kshell(cmd, usage, _MOD_VARNAME(ksh_command)); \
	static unsigned long _MOD_VARNAME(ksh_command)(ck::vec<ck::string> &args, void *data, int dlen)
