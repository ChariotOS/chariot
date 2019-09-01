#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <atom.h>
#include <dev/driver.h>
#include <dev/serial.h>
#include <idt.h>
#include <math.h>
#include <mem.h>
#include <module.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <printk.h>
#include <types.h>
#include <util.h>

#include <asm.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/blk_cache.h>
#include <dev/mbr.h>
#include <device.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <fs/vfs.h>
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
  node->walk_dir([&](const string& name, fs::vnoderef vn) -> bool {
    for (int i = 0; i < depth; i++) {
      printk("|");
      if (i == depth - 1) {
        printk(" - ");
      } else {
        printk("   ");
      }
    }

    printk("name=\"%s\"  inode=%d\n", name.get(), vn->index());

    if (name == "." || name == "..") return true;

    if (vn->index() != node->index())
      if (vn->is_dir()) walk_tree(vn, depth + 1);

    return true;
  });
}

void init_rootvfs(dev::device *dev) {

  if (dev == nullptr) {
    panic("couldnt find root device\n");
    return;
  }

  auto drive = dev->to_blk_dev();
  if (drive == nullptr) {
    panic("root device is not blk device\n");
    return;
  }

  // auto cache = dev::blk_cache(*drive, 64);

  auto rootfs = make_unique<fs::ext2>(*drive);

  if (!rootfs->init()) panic("failed to init ext2 on root disk\n");

  if (vfs::mount_root(move(rootfs)) < 0) panic("failed to mount rootfs");
}

uint32_t x; /* The state can be seeded with any value. */
/* Call next() to get 32 pseudo-random bits, call it again to get more bits. */
// It may help to make this inline, but you should see if it benefits your code.
uint32_t next(void) {
  uint32_t z = (x += 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

[[noreturn]] void kmain2(void) {
  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // now we want to clear out the initial mapping of the boot pages

  // initialize smp
  if (!smp::init()) panic("smp failed!\n");

  /*
   * initialize the PCI subsystem by walking the devices and creating an
   * internal representation that is faster to access later on
   */
  pci::init();

  /*
   * initialize the programmable interrupt timer
   */
  init_pit();

  /*
   * Set the PIT interrupt frequency to how many times per second it should fire
   */
  set_pit_freq(60);

  // finally, enable interrupts
  sti();

  assert(fs::devfs::init());

  // walk the kernel modules and run their init function
  initialize_kernel_modules();

  auto thedev = dev::open("ata1");
  if (thedev && false) {
    int nreads = 0;
    int stride = 512;
    for (size_t i = 0; i < mem_size(); i += stride) {
      char buf[stride];
      int rc = thedev->read(i, stride, buf);
      if (rc != stride) break;
      nreads++;
      // printk("%p\n", i);
      // hexdump(buf, stride, 32);
    }
    printk("nreads = %d\n", nreads);
  }

  // setup the root vfs
  init_rootvfs(thedev.get());

  fs::devfs::mount();

  auto node = vfs::open("/", 0);
  walk_tree(node);

  // spin forever
  printk("\n\nno more work. spinning.\n");
  while (1) {
    halt();
  }
}

extern "C" char chariot_welcome_start[];

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

extern void rtc_init(void);

// #define WASTE_TIME_PRINTING_WELCOME

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

