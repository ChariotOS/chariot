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

#include <asm.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/blk_cache.h>
#include <dev/mbr.h>
#include <device.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <func.h>
#include <map.h>
#include <phys.h>
#include <ptr.h>
#include <smp.h>
#include <string.h>
#include <uuid.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

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

typedef void (*func_ptr)(void);

extern "C" func_ptr __init_array_start[0], __init_array_end[0];

static void call_global_constructors(void) {
  for (func_ptr* func = __init_array_start; func != __init_array_end; func++)
    (*func)();
}

extern "C" void call_with_new_stack(void*, void*);

static void walk_tree(fs::vnoderef& node, int depth = 0) {
  if (depth == 0) {
    printk("/\n");
    depth++;
  }
  node->walk_dir([&](const string& name, fs::vnoderef vn) -> bool {
    if (name == "." || name == "..") return true;

    /*
    for (int i = 0; i < depth; i++) {
      printk("|");
      if (i == depth - 1) {
        printk(" - ");
      } else {
        printk("   ");
      }
    }
    */

    auto sz = 20;

    printk("%-20s\t", name.get());

    for (int i = 0; i < sz; i++) {
      char dst = 0;

      int read = vn->read(i, 1, &dst);
      if (read <= 0) break;
      printk("%02x ", dst);
    }
    printk("\n");

    if (vn->index() != node->index())
      if (vn->is_dir()) walk_tree(vn, depth + 1);

    return true;
  });
}

void do_drive_thing(string dev_name) {
  auto* dev = fs::devfs::get_device(dev_name);

  if (dev == nullptr) {
    printk("couldnt find device\n");
    return;
  }

  auto drive = dev->to_blk_dev();
  if (drive == nullptr) {
    printk("is not blk device\n");
    return;
  }

  auto cache = dev::blk_cache(*drive, 64);

  auto fs = fs::ext2(cache);

  if (!fs.init()) {
    printk("failed to init ext2\n");
    return;
  }

  auto root = fs.get_root_inode();

  if (!root) {
    printk("failed to get root node\n");
    return;
  }
  if (!root->is_dir()) {
    printk("root isn't dir\n");
    return;
  }

  walk_tree(root);
}

[[noreturn]] void kmain2(void) {
  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // initialize smp
  if (!smp::init()) panic("smp failed!\n");

  // initialize the PCI subsystem
  pci::init();

  // initialize the programmable interrupt timer
  init_pit();

  // and set the interval, or whatever
  set_pit_freq(100);

  // finally, enable interrupts
  sti();

  // walk the kernel modules and run their init function
  initialize_kernel_modules();

  // walk the disk
  do_drive_thing("disk1");

  // spin forever
  printk("no more work. spinning.\n");
  while (1) {
  }
}

extern "C" char chariot_welcome_start[];

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

extern void rtc_init(void);

#define WASTE_TIME_PRINTING_WELCOME

extern "C" [[noreturn]] void kmain(u64 mbd, u64 magic) {
  rtc_init();  // initialize the clock
  // initialize the serial "driver"
  serial_install();
  vga::init();

  vga::set_color(vga::color::white, vga::color::black);
  vga::clear_screen();

#ifdef WASTE_TIME_PRINTING_WELCOME
  printk("%s\n", chariot_welcome_start);
#endif
  printk("git: %s\n", GIT_REVISION);
  printk("\n");

  init_idt();

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();

  init_mem(mbd);

  init_kernel_virtual_memory();

#define STKSIZE (4096 * 8)
  void* new_stack = (void*)((u64)kmalloc(STKSIZE) + STKSIZE);

  void* new_main = p2v(kmain2);

  call_with_new_stack(new_stack, new_main);
  // ??
  panic("should not have gotten back here\n");

  while (1)
    ;
}

