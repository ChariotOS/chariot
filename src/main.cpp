#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <atom.h>
#include <cpu.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/blk_cache.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <dev/serial.h>
#include <device.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <fs/tmpfs.h>
#include <fs/vfs.h>
#include <func.h>
#include <idt.h>
#include <map.h>
#include <math.h>
#include <mem.h>
#include <module.h>
#include <paging.h>
#include <pci.h>
#include <phys.h>
#include <pit.h>
#include <printk.h>
#include <ptr.h>
#include <smp.h>
#include <string.h>
#include <types.h>
#include <util.h>
#include <uuid.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info* mod = __start__kernel_modules;
  int i = 0;
  while (mod != __stop__kernel_modules) {
    // printk("calling module %s\n", dev->name);
    mod->initfn();
    mod = &(__start__kernel_modules[++i]);
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
    for (int i = -1; i < depth; i++) {
      if (i == depth - 1) {
        printk("| - ");
      } else {
        printk("|   ");
      }
    }
    printk("%s", name.get());

    if (vn->is_dir()) printk("/");

    // printk("  %d", vn->index());

    printk("\n");

    if (name == "." || name == "..") return true;

    if (vn->index() != node->index())
      if (vn->is_dir()) walk_tree(vn, depth + 1);

    return true;
  });
}

void init_rootvfs(ref<dev::device> dev) {
  auto rootfs = make_unique<fs::ext2>(dev);
  if (!rootfs->init()) panic("failed to init ext2 on root disk\n");
  if (vfs::mount_root(move(rootfs)) < 0) panic("failed to mount rootfs");
}

[[noreturn]] void kmain2(void) {
  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // initialize the segments for this CPU
  cpu::seginit();

  // initialize smp
  if (!smp::init()) panic("smp failed!\n");

  // initialize the PCI subsystem by walking the devices and creating an
  // internal representation that is faster to access later on
  pci::init();

  // initialize the programmable interrupt timer
  init_pit();

  // Set the PIT interrupt frequency to how many times per second it should fire
  set_pit_freq(1000);

  // finally, enable interrupts
  sti();

  assert(fs::devfs::init());

  cpu::calc_speed_khz();

  // walk the kernel modules and run their init function
  initialize_kernel_modules();

  // open up the disk device for the root filesystem
  auto rootdev = dev::open("ata1");

  // setup the root vfs
  init_rootvfs(rootdev);

  // mount the devfs
  fs::devfs::mount();

  // setup the tmp filesystem
  vfs::mount(make_unique<fs::tmp>(), "/tmp");
  auto tmp = vfs::open("/tmp", 0);
  tmp->touch("foo", fs::inode_type::file, 0777);
  tmp->mkdir("bar", 0777);

  u64 ind = 0;

  auto d = dev::open("kbd");

  for (u64 addr = 0; true; addr += sizeof(char)) {
    u8 c = 0x00;
    int read = d->read(addr, sizeof(u32), &c);

    if (read != 0) {
      printk("%02x ", c);
      vga::set_pixel(ind, vga::rgb(0, 0xFF, 0xFF));
    }

    ind++;
    ind %= vga::width() * vga::height();
  }

  // auto node = vfs::open("/", 0);
  // walk_tree(node);

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

#ifdef WASTE_TIME_PRINTING_WELCOME
  printk("%s\n", chariot_welcome_start);
  printk("git: %s\n", GIT_REVISION);
  printk("\n");
#endif

  init_idt();

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();
  init_mem(mbd);
  init_kernel_virtual_memory();

#define STKSIZE (4096 * 8)
  void* new_stack = (void*)((u64)kmalloc(STKSIZE) + STKSIZE);

  // call the next phase main with the new allocated stack
  call_with_new_stack(new_stack, (void*)kmain2);
  // ??
  panic("should not have gotten back here\n");

  while (1)
    ;
}

