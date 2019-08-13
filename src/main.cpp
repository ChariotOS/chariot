#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <dev/ata.h>
#include <dev/serial.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <multiboot.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <posix.h>
#include <printk.h>
#include <types.h>

#include <asm.h>
#include <device.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <func.h>
#include <phys.h>
#include <ptr.h>
#include <string.h>
#include <vec.h>
#include <vga.h>

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

extern void print_heap(void);

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
      "Nick's Operating System",
      "Neat Operating System",
      "Nifty Operating System",
      "New Operating System",
      "GNU Terry Pratchett",
      "Nitrous Oxide",
      "Now open source!",
      "Not officially sanctioned!",
      "WOW!",
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

[[noreturn]] void kmain2(void) {
  u64 addr = 0;
  paging::map(addr + KERNEL_VIRTUAL_BASE, addr, paging::pgsize::page);
  paging::map(addr, addr, paging::pgsize::page);

  tlb_flush();

  addr++;

  auto* dst = (u8*)(addr + KERNEL_VIRTUAL_BASE);

  dst[0] = 0xFA;
  printk("%02x\n", *(u8*)addr);
  dst[0] = 0xFE;
  printk("%02x\n", *(u8*)addr);

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

  int i;
  printk("%p\n", &i);
  while (1) {
    phys::alloc();
  }

  // finally, enable interrupts
  sti();

  // print_heap();
  auto drive = dev::ata(0x1F0, true);

  if (drive.identify()) {
    auto fs = fs::ext2(drive);
    if (fs.init()) {
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
      test("/kernel/");
      test("/kernel/fs/");
      test("/kernel/fs/ext2.cpp");
      test("/kernel/fs/ext2.cpp/foo");
    }
  }

  // spin forever
  printk("no more work. spinning.\n");
  while (1) {
  }

  printk("done\n");
  while (1)
    ;
}

extern "C" void call_with_new_stack(void*, void*);

extern "C" int kmain(u64 mbd, u64 magic) {
  // initialize the serial "driver"
  serial_install();
  vga::init();

  vga::set_color(vga::color::white, vga::color::black);
  vga::clear_screen();

  init_idt();

  printk(NOS_WELCOME);
  printk("   Nick Wanninger (c) 2019 | Illinois Institute of Technology\n");
  printk("   nOS: %s\n", splash());
  printk("   git: %s\n", GIT_REVISION);
  printk("\n\n");

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();

  // at this point, we are still mapped with the 2mb large pages.
  // init_mem will replace this with a more fine-grained 4k page system by
  // mapping kernel memory 1:1

  init_mem(mbd);

  init_kernel_virtual_memory();

#define STKSIZE (4096 * 8)
  void* new_stack = (void*)((u64)kmalloc(STKSIZE) + STKSIZE);

  call_with_new_stack(new_stack, (void*)kmain2);

  return 0;
}

