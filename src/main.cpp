#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <dev/ata.h>
#include <dev/serial.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <posix.h>
#include <printk.h>
#include <types.h>
#include <multiboot.h>

#include <asm.h>
#include <device.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <func.h>
#include <phys.h>
#include <ptr.h>
#include <string.h>
#include <vec.h>

extern int kernel_end;

u64 fib(u64 n) {
  if (n < 2) return 1;

  return fib(n - 1) + fib(n - 2);
}

/*
static u64 strlen(char *str) {
  u64 i = 0;
  for (i = 0; str[i] != '\0'; i++)
    ;
  return i;
}
*/

// in src/arch/x86/sse.asm
extern "C" void enable_sse();

void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info* dev = __start__kernel_modules;
  int i = 0;
  while (dev != __stop__kernel_modules) {
    dev->initfn();
    dev = &(__start__kernel_modules[++i]);
  }
}

/* These magic symbols are provided by the linker.  */
extern void (*__preinit_array_start[])(void) __attribute__((weak));
extern void (*__preinit_array_end[])(void) __attribute__((weak));
extern void (*__init_array_start[])(void) __attribute__((weak));
extern void (*__init_array_end[])(void) __attribute__((weak));

static void call_global_constructors(void) {
  size_t count;
  size_t i;

  count = __preinit_array_end - __preinit_array_start;
  for (i = 0; i < count; i++) __preinit_array_start[i]();

  // _init();

  count = __init_array_end - __init_array_start;
  for (i = 0; i < count; i++) __init_array_start[i]();
}

class Foo {
 public:
  Foo() { printk("Foo ctor\n"); }
  ~Foo() { printk("Foo dtor\n"); }
};

Foo f;

extern "C" void play_sound(uint32_t nFrequence) {
  uint32_t Div;
  uint8_t tmp;

  // Set the PIT to the desired frequency
  Div = 1193180 / nFrequence;

  printk("%d\n", Div);
  outb(0x43, 0xb6);
  outb(0x42, (uint8_t)(Div));
  outb(0x42, (uint8_t)(Div >> 8));

  // And play the sound using the PC speaker
  tmp = inb(0x61);
  if (tmp != (tmp | 3)) {
    outb(0x61, tmp | 3);
  }
}

static const char* splash(void) {
  static const char* msgs[] = {
      "Nick's Operating System", "Neat Operating System",
      "Nifty Operating System",  "New Operating System",
      "GNU Terry Pratchett",   "Nitrous Oxide",
  };
  // not the best way to randomly seed this stuff, but what am I going to do.
  return msgs[rdtsc() % (sizeof(msgs) / sizeof(msgs[0]))];
}

#define NOS_WELCOME               \
  "             ____  _____\n"    \
  "      ____  / __ \\/ ___/\n"   \
  "     / __ \\/ / / /\\__ \\ \n" \
  "    / / / / /_/ /___/ / \n"    \
  "   /_/ /_/\\____//____/  \n"   \
  "                        \n"


#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif
extern void vga_init(void);
extern "C" int kmain(u64 mbd, u64 magic) {
  // initialize the serial "driver"
  serial_install();
  vga_init();

  init_idt();

  printk("Welcome to\n");
  printk(NOS_WELCOME);
  printk("   nOS: %s\n", splash());
  printk("   git: %s\n", GIT_REVISION);

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();
  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by
  // mapping kernel memory 1:1
  init_mem(mbd);

  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // initialize the PCI subsystem
  pci::init();

  // now that we have a decent programming enviroment, we can go through and
  // register all the kernel modules
  // NOTE: kernel module init code should not do any work outside of registering
  //       andlers, as the enviroment they are run in is fairly minimal and have
  //       no complex things like processes or devices.
  initialize_kernel_modules();

  // initialize the programmable interrupt timer
  init_pit();
  // and set the interval, or whatever
  set_pit_freq(100);

  // finally, enable interrupts
  sti();

  if (0) {
    auto drive = dev::ata(0x1F0, true);

    if (drive.identify()) {
      auto fs = fs::ext2(drive);
      fs.init();

      auto test = [&](const char* path) {
        auto inode = fs.open(string(path), 0);
        if (inode) {
          printk("found '%s' at inode %d\n", path, inode->index());
        } else {
          printk("Could not find inode\n");
        }
      };

      test("/");
      test("/kernel");
      test("/kernel/src/");
      test("/kernel/src/fs/");
      test("/kernel/src/fs/ext2.cpp");
      test("/kernel/src/fs/ext2.cpp/foo");
    }
  }

  play_sound(1000);

  printk("done!\n");

  // now try and exit.
  // write to the mobo exit port (special port)
  // outb(0xE817, 0);

  // simply hltspin
  while (1) {
  }
  return 0;
}

