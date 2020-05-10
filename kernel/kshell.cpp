#include <kshell.h>
#include <errno.h>
#include <map.h>
#include <syscall.h>
#include <cpu.h>

static map<string, kshell::handler> commands;




void kshell::add(string command, kshell::handler h) {
	commands[command] = h;
}


unsigned long kshell::call(string command, vec<string> args, void *data, size_t dlen) {
	if (commands.contains(command)) {
		return commands[command](args, data, dlen);
	}
	return -ENOENT;
}


unsigned long sys::kshell(char *cmd, int argc, char **argv, void *data, size_t dlen) {

	// validate the arguments
	if (!curproc->mm->validate_string(cmd)) {
		return -1;
	}
	if (!curproc->mm->validate_pointer(argv, sizeof(char*) * argc, VALIDATE_READ)) {
		return -1;
	}

	for (int i = 0; i < argc; i++) {
		if (!curproc->mm->validate_string(argv[i])) {
			return -1;
		}
	}

	if (data != NULL) {
		if (!curproc->mm->validate_pointer(data, dlen, VALIDATE_READ | VALIDATE_WRITE)) {
			return -1;
		}
	}

	vec<string> args;
	for (int i = 0; i < argc; i++) {
		args.push(argv[i]);
	}

	return kshell::call(cmd, args, data, dlen);
}
