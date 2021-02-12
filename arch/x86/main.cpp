#include <arch.h>
#include <asm.h>
#include <cpu.h>
#include <cpuid.h>
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
#include <single_list.h>
#include <syscall.h>
#include <time.h>
#include <types.h>
#include <util.h>
#include <vga.h>
#include <x86/cpuid.h>
#include <x86/fpu.h>
#include <x86/smp.h>
#include "acpi/acpi.h"

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void *, void *);


static void kmain2(void);

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);
extern void rtc_late_init(void);

static uint64_t mbd;
void serial_install(void);

extern "C" [[noreturn]] void kmain(u64 mbd, u64 magic) {
  serial_install();
  rtc_init();
  extern u8 boot_cpu_local[];
  cpu::seginit(boot_cpu_local);

  arch_mem_init(mbd);

  ::mbd = mbd;

  // initialize the video display
  vga::early_init(mbd);

  void *new_stack = (void *)((u64)malloc(STKSIZE) + STKSIZE);
  call_with_new_stack(new_stack, (void *)kmain2);
  while (1) panic("should not have gotten back here\n");
}


typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];

// kernel/init.cpp
int kernel_init(void *);


// ms per tick
#define TICK_FREQ 100


static void kmain2(void) {
  irq::init();
  fpu::init();

  /* Call all the global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }

  kargs::init(mbd);
  smp::init();

#ifdef CONFIG_ACPI
  if (!acpi::init(mbd)) panic("acpi init failed!\n");
#endif

  cpuid::detect_cpu();

  init_pit();
  set_pit_freq(TICK_FREQ);
  KINFO("Initialized PIT\n");

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // create the initialization thread.
  sched::proc::create_kthread("[kinit]", kernel_init);

  // the first cpu is the timekeeper
  cpus[0].timekeeper = true;

  KINFO("starting scheduler\n");
  sched::run();

  panic("sched::run() returned\n");
  // [noreturn]
}


#define ACPI_EBDA_PTR_LOCATION 0x0000040E /* Physical Address */
#define ACPI_EBDA_PTR_LENGTH 2
#define ACPI_EBDA_WINDOW_SIZE 1024
#define ACPI_HI_RSDP_WINDOW_BASE 0x000E0000 /* Physical Address */
#define ACPI_HI_RSDP_WINDOW_SIZE 0x00020000
#define ACPI_RSDP_SCAN_STEP 16

int kernel_init(void *) {
  rtc_late_init();

  // at this point, the pit is being used for interrupts,
  // so we should go setup lapic for that
  smp::lapic_init();

  // start up the extra cpu cores
  smp::init_cores();

  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");
  net::start();

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_builtin_modules();
  KINFO("kernel modules initialized\n");


	KINFO("Bootup complete. It is now safe to move about the cabin.\n");





#ifdef CONFIG_USERSPACE

  auto root_name = kargs::get("root", "/dev/disk0p1");
  assert(root_name);

  int mnt_res = vfs::mount(root_name, "/", "ext2", 0, NULL);
  if (mnt_res != 0) {
    panic("failed to mount root. Error=%d\n", -mnt_res);
  }

  auto kproc = sched::proc::kproc();
  kproc->root = fs::inode::acquire(vfs::get_root());
  kproc->cwd = fs::inode::acquire(vfs::get_root());


  string init_paths = kargs::get("init", "/bin/init");

  auto paths = init_paths.split(',');

  pid_t init_pid = sched::proc::spawn_init(paths);


  sys::waitpid(init_pid, NULL, 0);
  panic("init died!\n");

  if (init_pid == -1) {
    KERR("failed to create init process\n");
    KERR("check the grub config and make sure that the init command line arg\n");
    KERR("is set to a comma seperated list of possible paths.\n");
  }

#endif

  // yield back to scheduler, we don't really want to run this thread anymore
  while (1) {
    arch_halt();
    sched::yield();
  }

  panic("main kernel thread reached unreachable code\n");
}
