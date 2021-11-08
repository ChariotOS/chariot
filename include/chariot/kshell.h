#pragma once
#include <ck/vec.h>
#include <ck/string.h>

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
	void run(void);


	// if the kernel shell is currently being run on a thread.
	bool active(void);

	// feed input (from the console, most likely)
	void feed(size_t sz, char *buf);

};  // namespace kshell
