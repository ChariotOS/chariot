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
#include <ck/single_list.h>
#include <syscall.h>
#include <time.h>
#include <types.h>
#include <util.h>
#include <vga.h>
#include <x86/cpuid.h>
#include <x86/fpu.h>
#include <x86/smp.h>
#include <crypto.h>
#include "acpi/acpi.h"
#include <ck/iter.h>
#include <fs/tmpfs.h>
#include <fs/devfs.h>

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void*, void*);


typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
int kernel_init(void*);

// ms per tick
#define TICK_FREQ 100



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

  // allocate the CPU core for the bootstrap processor and
  // register it with the kernel
  cpu::Core cpu;
  extern u8 boot_cpu_local[];
  cpu::seginit(&cpu, boot_cpu_local);

  cpu.timekeeper = false;
  core().timekeeper = true;
  cpu.primary = true;



  arch_mem_init(mbd);

  ::mbd = mbd;

  // initialize the video display
  vga::early_init(mbd);


  /* Call all the global constructors */
  for (func_ptr* func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }


  irq::init();
  fpu::init();
  kargs::init(mbd);

  rtc_late_init();

#ifdef CONFIG_ACPI
  if (!acpi::init(mbd)) panic("acpi init failed!\n");
#endif

#ifdef CONFIG_SMP
  smp::init();
#endif

  cpuid::detect_cpu();
  // initialize the bootstrap processor's APIC
  core().apic.init();


  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // create the initialization thread.
  sched::proc::create_kthread("[kinit]", kernel_init);


  KINFO("starting scheduler\n");
  sched::run();

  panic("sched::run() returned\n");

  while (1) {
  }
}


class MyDevice : public fs::CharDeviceNode {

};

int kernel_init(void*) {
  // start up the extra cpu cores
#ifdef CONFIG_SMP
  smp::init_cores();
#endif



  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");
  net::start();

	// Init the virtual filesystem and mount a tmpfs and devfs to / and /dev
	vfs::init_boot_filesystem();


	MyDevice n;
	n.bind("mydev");

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_builtin_modules();
  KINFO("kernel modules initialized\n");

  sched::proc::create_kthread("[reaper]", Process::reaper);


  mb2::find<struct multiboot_tag_module>(::mbd, MULTIBOOT_TAG_TYPE_MODULE, [&](auto* module) {
    auto size = module->mod_end - module->mod_start;
    printk(KERN_INFO "Found a module %p-%p\n", p2v(module->mod_start), p2v(module->mod_end));
  });


  KINFO("Bootup complete. It is now safe to move about the cabin.\n");
  auto root_name = kargs::get("root", "/dev/disk0p1");
  if (!root_name) {
    KERR("Failed to mount root...\n");
    while (1) {
      arch_halt();
      sched::yield();
    }
  }

  assert(root_name);


#ifndef CONFIG_ENABLE_USERSPACE
  KINFO("Userspace disabled. Starting kernel shell\n");
  kshell::run();
#endif

  auto kproc = sched::proc::kproc();
  kproc->root = vfs::get_root();
  kproc->cwd = vfs::get_root();


  ck::string init_paths = kargs::get("init", "/bin/init");

  auto paths = init_paths.split(',');

  auto init_pid = sched::proc::spawn_init(paths);
  printk("here %d\n", init_pid);

  sys::waitpid(init_pid, NULL, 0);
  panic("init died!\n");

  if (init_pid == -1) {
    KERR("failed to create init process\n");
    KERR("check the grub config and make sure that the init command line arg\n");
    KERR("is set to a comma seperated list of possible paths.\n");
  }

  // yield back to scheduler, we don't really want to run this thread anymore
  while (1) {
    arch_halt();
    sched::yield();
  }

  panic("main kernel thread reached unreachable code\n");
}
