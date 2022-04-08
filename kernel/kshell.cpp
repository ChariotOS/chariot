#include <kshell.h>
#include <errno.h>
#include <ck/map.h>
#include <syscall.h>
#include <cpu.h>
#include <chan.h>
#include <process.h>
#include <console.h>
#include <module.h>
#include <util.h>

struct command {
  kshell::handler handler;
  ck::string usage;
};
static ck::map<ck::string, command> commands;




void kshell::add(ck::string command, ck::string usage, kshell::handler h) { commands[command] = {h, usage}; }


unsigned long kshell::call(ck::string command, ck::vec<ck::string> args, void *data, size_t dlen) {
  if (commands.contains(command)) {
    return commands[command].handler(args, data, dlen);
  }
  return -ENOENT;
}


unsigned long sys::kshell(void) {
  // make this thread run the kshell :)
  kshell::run("$>");
  return 0;
}


static bool active = false;
static chan<char> kshell_pipe;
static ck::string last_command;



static void exec(const ck::string &raw, bool reexec = true) {
  // split the command on spaces
  auto parts = raw.split(' ', false);
  if (parts.size() >= 1) {
    last_command.clear();
    last_command = raw;
    ck::string cmd = parts[0];
    parts.remove(0);


    if (commands.contains(cmd)) {
      auto res = kshell::call(cmd, parts, nullptr, 0);
      if (res) {
        printf(" -> %lu\n", res);
      } else {
        printf("\n");
      }
    } else {
      KERR("Command '%s' not found\n", cmd.get());
    }
  } else if (reexec) {
    ck::string cmd = last_command;
    exec(cmd, false);
    return;
  }
}

void kshell::eval(const ck::string &raw) {
	return exec(raw, false);
}

bool kshell::active(void) { return ::active; }

void kshell::feed(size_t sz, char *buf) {
  for (size_t i = 0; i < sz; i++) {
    auto c = buf[i];
    kshell_pipe.send(move(c), false);
  }
}

void kshell::run(const char *prompt) {
  ::active = true;

  KINFO("Dropping into kernel shell. Enter 'help' for more information\n");
  while (kshell::active()) {
    printf("%s ", prompt);
    ck::string input;
    while (1) {
      int c = kshell_pipe.recv();

      // on delete, backspace
      if (c == CONS_DEL) {
        if (input.size() > 0) {
          console::putc(c, true);
        }
        input.pop();
        continue;
      }

      if (c == '\n' || c == '\r') {
        console::putc('\n', true);
        // :)
        if (input == "exit") {
          ::active = false;
          break;
        }
        exec(input);
        input.clear();
        // break from the input loop
        break;
      }

      console::putc(c, true);
      input.push(c);
    }
  }

  ::active = false;
}




static unsigned long kshell_help(ck::vec<ck::string> &args, void *data, int dlen) {
  // printf("Kernel shell help:\n");

  printf("Available Commands\n");
  for (auto cmd : commands) {
    printf(" - %s: '%s'\n", cmd.key.get(), cmd.value.usage.get());
  }
  return 0;
}
module_init_kshell("help", "help", kshell_help);



static bool parse_arg(ck::string &s, unsigned long *val) {
  if (s.size() > 2) {
    if (s[0] == '0' && s[1] == 'x') {
      if (s.scan("0x%lx", val) == 1) {
        return true;
      }
      return false;
    }
  }
  if (s.scan("%ld", val) == 1) {
    return true;
  }
  return false;
}

ksh_def("x", "x address length") {
  unsigned long address = 0;
  unsigned long size = 0;

  if (args.size() != 2) {
    printf("Usage: x <address> <length>\n");
    return 1;
  }


  if (!parse_arg(args[0], &address)) {
    printf("malformed address\n");
    return 1;
  }


  if (!parse_arg(args[1], &size)) {
    printf("malformed size\n");
    return 1;
  }


  hexdump(p2v(address), size, true);
  return 0;
}
