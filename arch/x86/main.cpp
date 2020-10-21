#include <cpu.h>
#include <cpuid.h>
#include <dev/serial.h>
#include <elf/loader.h>
#include <fs/ext2.h>
#include <fs/vfs.h>
#include <kargs.h>
#include <kshell.h>
#include <module.h>
#include <multiboot.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <pci.h>
#include <pit.h>
#include <syscall.h>
#include <time.h>
#include <types.h>
#include <util.h>
#include <vga.h>
#include "cpuid.h"
#include "fpu.h"
#include "smp.h"

#include <arch.h>


void dump_multiboot(struct multiboot_info *mboot_ptr) {
  KINFO("MULTIBOOT header at 0x%x:\n", (uintptr_t)mboot_ptr);
  KINFO("Flags : 0x%x\n", mboot_ptr->flags);
  KINFO("Mem Lo: 0x%x\n", mboot_ptr->mem_lower);
  KINFO("Mem Hi: 0x%x\n", mboot_ptr->mem_upper);
  KINFO("Boot d: 0x%x\n", mboot_ptr->boot_device);
  KINFO("cmdlin: 0x%x\n", mboot_ptr->cmdline);
  KINFO("Mods  : 0x%x\n", mboot_ptr->mods_count);
  KINFO("Addr  : 0x%x\n", mboot_ptr->mods_addr);
  // KINFO("ELF n : 0x%x", mboot_ptr->num);
  // KINFO("ELF s : 0x%x", mboot_ptr->size);
  // KINFO("ELF a : 0x%x", mboot_ptr->addr);
  // KINFO("ELF h : 0x%x", mboot_ptr->shndx);
  KINFO("MMap  : 0x%x\n", mboot_ptr->mmap_length);
  KINFO("Addr  : 0x%x\n", mboot_ptr->mmap_addr);
  KINFO("Drives: 0x%x\n", mboot_ptr->drives_length);
  KINFO("Addr  : 0x%x\n", mboot_ptr->drives_addr);
  KINFO("Config: 0x%x\n", mboot_ptr->config_table);
  KINFO("Loader: 0x%x\n", mboot_ptr->boot_loader_name);
  KINFO("APM   : 0x%x\n", mboot_ptr->apm_table);
  KINFO("VBE Co: 0x%x\n", mboot_ptr->vbe_control_info);
  KINFO("VBE Mo: 0x%x\n", mboot_ptr->vbe_mode_info);
  KINFO("VBE In: 0x%x\n", mboot_ptr->vbe_mode);
  KINFO("VBE se: 0x%x\n", mboot_ptr->vbe_interface_seg);
  KINFO("VBE of: 0x%x\n", mboot_ptr->vbe_interface_off);
  KINFO("VBE le: 0x%x\n", mboot_ptr->vbe_interface_len);
  if (mboot_ptr->flags & (1 << 2)) {
    KINFO("Started with: %s\n", (char *)p2v(mboot_ptr->cmdline));
  }
  if (mboot_ptr->flags & (1 << 9)) {
    KINFO("Booted from: %s\n", (char *)p2v(mboot_ptr->boot_loader_name));
  }
  if (mboot_ptr->flags & (1 << 0)) {
    KINFO("%dkB lower memory\n", mboot_ptr->mem_lower);
    int mem_mb = mboot_ptr->mem_upper / 1024;
    KINFO("%dkB higher memory (%dMB)\n", mboot_ptr->mem_upper, mem_mb);
  }
  if (mboot_ptr->flags & (1 << 3)) {
    KINFO("Found %d module(s).\n", mboot_ptr->mods_count);
    if (mboot_ptr->mods_count > 0) {
      uint32_t i;
      for (i = 0; i < mboot_ptr->mods_count; ++i) {
        uint32_t module_start = *((uint32_t *)p2v(mboot_ptr->mods_addr + 8 * i));
        uint32_t module_end = *(uint32_t *)p2v(mboot_ptr->mods_addr + 8 * i + 4);
        KINFO("Module %d is at 0x%x:0x%x\n", i + 1, module_start, module_end);
      }
    }
  }
}

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
  rtc_init();
  extern u8 boot_cpu_local[];
  cpu::seginit(boot_cpu_local);

  arch::mem_init(mbd);

  mbinfo = (struct multiboot_info *)(u64)p2v(mbd);


  // initialize the video display
  vga::early_init(mbinfo);

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
  fpu::init();

  call_global_constructors();

  kargs::init(mbinfo);

  if (!kargs::exists("nosmp")) {
    if (!smp::init()) panic("smp failed!\n");
    KINFO("Discovered SMP Cores\n");
  }

  dump_multiboot(mbinfo);

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




#define cpuid(in, a, b, c, d) __asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(in));

int kernel_init(void *) {
  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");


  // at this point, the pit is being used for interrupts,
  // so we should go setup lapic for that
  smp::lapic_init();
  syscall_init();


  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_builtin_modules();
  KINFO("kernel modules initialized\n");

  if (!kargs::exists("nosmp")) {
    // start up the extra cpu cores
    smp::init_cores();
  }




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


  if (vfs::mount("none", "/tmp", "tmpfs", 0, NULL) != 0) {
    panic("failed to mount tmpfs");
  }

  // setup stdio stuff for the kernel (to be inherited by spawn)
  int fd = sys::open("/dev/console", O_RDWR, 0);
  assert(fd == 0);

  sys::dup2(fd, 1);
  sys::dup2(fd, 2);


  auto kproc = sched::proc::kproc();
  kproc->root = fs::inode::acquire(vfs::get_root());
  kproc->cwd = fs::inode::acquire(vfs::get_root());


  net::start();


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
    KERR("check the grub config and make sure that the init command line arg\n");
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
  while (1) {
  }
}
