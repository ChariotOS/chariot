#include <kargs.h>
#include <string.h>


#include <map.h>
#include <printk.h>
#include <vec.h>

static map<string, string> cmdline_map;

void kargs::init(struct multiboot_info *info) {
  string cmdline((char *)(u64)p2v(info->cmdline));


  KINFO("Command line arguments:\n");
  int i = 0;
  for (auto arg : cmdline.split(' ')) {
    KINFO("  args[%d]='%s'\n", i++, arg.get());
    auto parts = arg.split('=');

    if (parts.size() == 2) {
      cmdline_map.set(parts[0], parts[1]);
    } else {
      KWARN("Kernel argument '%s' not of the form key=value. Discarded.\n", arg.get());
    }
  }
}

const char *kargs::get(const char *keyp, const char *def) {
  string key = keyp;  // optimize away the string copies
  if (cmdline_map.contains(key)) {
    return cmdline_map[key].get();
  }
  return def;
}
