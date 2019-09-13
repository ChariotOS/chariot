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
#include <keycode.h>
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
#include <sched.h>
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

  // initialize smp
  // if (!smp::init()) panic("smp failed!\n");

  // initialize the PCI subsystem by walking the devices and creating an
  // internal representation that is faster to access later on
  pci::init();

  // initialize the programmable interrupt timer
  init_pit();

  // Set the PIT interrupt frequency to how many times per second it should fire
  set_pit_freq(1000);

  assert(fs::devfs::init());

  cpu::calc_speed_khz();

  // initialize the scheduler
  assert(sched::init());

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

  /*
  auto node = vfs::open("/", 0);
  walk_tree(node);
  */

  sched::spawn_kernel_task("rainbow_shit", []() -> void {
    int c = 0;
    while (1) {
      c++;
      int width = 600;
      u32 col = vga::hsl((c % width) / (float)width, 1, 0.5);
      for_range(y, 0, vga::height()) {
        vga::set_pixel(c % vga::width(), y, col);
      }
    }
  });

  sched::spawn_kernel_task("task1", []() -> void {
    while (1) {
      printk("1");
    }
  });

  sched::spawn_kernel_task("task2", []() -> void {
    while (1) {
      printk("2");
    }
  });

  sched::spawn_kernel_task("task3", []() -> void {
    while (1) {
      // auto p = kmalloc(40);
      printk("3");
      // kfree(p);
    }
  });

  sched::run();

  // spin forever
  panic("should not have returned here\n");
  while (1) {
    halt();
  }
}

extern "C" char chariot_welcome_start[];

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);

extern "C" u8 boot_cpu_local[];

// #define WASTE_TIME_PRINTING_WELCOME

extern "C" [[noreturn]] void kmain(u64 mbd, u64 magic) {
  /*
   * Initialize the real-time-clock
   */
  rtc_init();

  /*
   * TODO: replace this serial driver with a major/minor device driver
   */
  serial_install();

  /*
   * Initialize early VGA state
   */
  vga::init();

#ifdef WASTE_TIME_PRINTING_WELCOME
  printk("%s\n", chariot_welcome_start);
  printk("git: %s\n", GIT_REVISION);
  printk("\n");
#endif

  /*
   * using the boot cpu local information page, setup the CPU and
   * fd segment so we can use CPU specific information early on
   */
  cpu::seginit(boot_cpu_local);

  /**
   * setup interrupts
   */
  init_idt();

  /**
   * enable SSE extensions now that FP Faults can be caught by the OS
   *
   * TODO: dont use FP in the kernel.
   */
  enable_sse();

  /**
   * detect memory and setup the physical address space free-list
   */
  init_mem(mbd);
  /**
   * startup the high-kernel virtual mapping and the heap allocator
   */
  init_kernel_virtual_memory();

  void* new_stack = (void*)((u64)kmalloc(STKSIZE) + STKSIZE);

  // call the next phase main with the new allocated stack
  call_with_new_stack(new_stack, (void*)kmain2);

  // ?? - gotta loop forever here to make gcc happy. using [[gnu::noreturn]]
  // requires that it never returns...
  while (1) panic("should not have gotten back here\n");
}

