#include <kargs.h>
#include <ck/string.h>


#include <ck/map.h>
#include <printk.h>
#include <ck/vec.h>

static ck::map<ck::string, ck::string> cmdline_map;

void kargs::init(uint64_t mbd) {
  mb2::find<struct multiboot_tag_string>(mbd, MULTIBOOT_TAG_TYPE_CMDLINE, [](auto *info) {
    ck::string cmdline((char *)(u64)p2v(info->string));
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
  });
}

const char *kargs::get(const char *keyp, const char *def) {
  ck::string key = keyp;  // optimize away the ck::string copies
  if (cmdline_map.contains(key)) {
    return cmdline_map[key].get();
  }
  return def;
}


bool kargs::exists(const char *name) { return cmdline_map.contains(name); }
