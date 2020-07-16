#include <cpu.h>
#include <dev/serial.h>
#include <elf/loader.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <kargs.h>
#include <kshell.h>
#include <module.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <pci.h>
#include <pit.h>
#include <syscall.h>
#include <types.h>
#include <util.h>
#include <vga.h>
#include "smp.h"

#include <arch.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void *, void *);


struct multiboot_info *mbinfo;
static void kmain2(void);

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif





/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);

extern "C" [[noreturn]] void kmain(u64 mbd, u64 magic) {
  serial_install();
	printk("hello, world\n");

  rtc_init();

  extern u8 boot_cpu_local[];
  cpu::seginit(boot_cpu_local);

  arch::mem_init(mbd);

  mbinfo = (struct multiboot_info *)(u64)p2v(mbd);
  void *new_stack = (void *)((u64)kmalloc(STKSIZE) + STKSIZE);
  call_with_new_stack(new_stack, (void *)kmain2);
  while (1) panic("should not have gotten back here\n");
}

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];

static void call_global_constructors(void) {
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }
}

// kernel/init.cpp
int kernel_init(void *);


// ms per tick
#define TICK_FREQ 100


static void kmain2(void) {
  irq::init();
  enable_sse();

  call_global_constructors();

  kargs::init(mbinfo);

  if (!smp::init()) panic("smp failed!\n");
  KINFO("Discovered SMP Cores\n");



  init_pit();
  set_pit_freq(TICK_FREQ);
  KINFO("Initialized PIT\n");

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // create the initialization thread.
  sched::proc::create_kthread("[kinit]", kernel_init);

  KINFO("starting scheduler\n");
  sched::run();

  panic("sched::run() returned\n");
  // [noreturn]
}


int kernel_init(void *) {
  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");

  vga::early_init(mbinfo);

  // at this point, the pit is being used for interrupts,
  // so we should go setup lapic for that
  smp::lapic_init();
  syscall_init();


  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_builtin_modules();
  KINFO("kernel modules initialized\n");




  // start up the extra cpu cores
  smp::init_cores();

  sched::proc::create_kthread("[net]", net::task);

  // open up the disk device for the root filesystem
  // auto rootdev = dev::open(kargs::get("root", "ata0p1"));
  // assert(rootdev);

  auto root_name = kargs::get("root", "/dev/ata0p1");
  assert(root_name);


  int mnt_res = vfs::mount(root_name, "/", "ext2", 0, NULL);
  if (mnt_res != 0) {
    panic("failed to mount root. Error=%d\n", -mnt_res);
  }

  if (vfs::mount("none", "/dev", "devfs", 0, NULL) != 0) {
    panic("failed to mount devfs");
  }

  auto kimg = vfs::fdopen("/boot/chariot.elf", O_RDONLY);

  if (false && kimg) {
    elf::each_symbol(kimg, [](const char *name, off_t addr) {
      // printk("%p %s\n", addr, name);
      return true;
    });
  }



  // setup stdio stuff for the kernel (to be inherited by spawn)
  int fd = sys::open("/dev/console", O_RDWR);
  assert(fd == 0);

  sys::dup2(fd, 1);
  sys::dup2(fd, 2);


  auto kproc = sched::proc::kproc();
  kproc->root = fs::inode::acquire(vfs::get_root());
  kproc->cwd = fs::inode::acquire(vfs::get_root());

  string init_paths = kargs::get("init", "/bin/init");

  auto paths = init_paths.split(',');

  pid_t init_pid = sched::proc::spawn_init(paths);

  /*
  while (1) {
          arch::us_this_second();
  }
  */
  sys::waitpid(init_pid, NULL, 0);
  panic("init died!\n");

  if (init_pid == -1) {
    KERR("failed to create init process\n");
    KERR(
        "check the grub config and make sure that the init command line arg\n");
    KERR("is set to a comma seperated list of possible paths.\n");
  }

  // yield back to scheduler, we don't really want to run this thread anymore
  while (1) {
    arch::halt();
    sched::yield();
  }

  panic("main kernel thread reached unreachable code\n");
}


extern "C" void _hrt_start(void) {
	kmain(0, 0);
	while (1) {}
}
