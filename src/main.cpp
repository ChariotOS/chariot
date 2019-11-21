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
#include <elf/loader.h>
#include <fs/devfs.h>
#include <fs/ext2.h>
#include <fs/fat.h>
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
#include <mouse_packet.h>
#include <paging.h>
#include <pci.h>
#include <phys.h>
#include <pit.h>
#include <printk.h>
#include <process.h>
#include <ptr.h>
#include <sched.h>
#include <smp.h>
#include <string.h>
#include <task.h>
#include <types.h>
#include <util.h>
#include <uuid.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void*, void*);

// HACK: not real kernel modules right now, just basic function pointers in an
// array statically.
// TODO: some initramfs or something with a TAR file and load them in as kernel
// modules or something :)
void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info* mod = __start__kernel_modules;
  int i = 0;
  while (mod != __stop__kernel_modules) {
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

void init_rootvfs(ref<dev::device> dev) {
  auto rootfs = make_unique<fs::ext2>(dev);
  if (!rootfs->init()) panic("failed to init fs on root disk\n");
  if (vfs::mount_root(move(rootfs)) < 0) panic("failed to mount rootfs");
}

atom<int> nidles = 0;
static int idle_task(void* arg) {
  while (1) {
    // increment nidels
    nidles.store(nidles.load() + 1);
    halt();
  }
}

static void kmain2(void);

extern "C" char chariot_welcome_start[];

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);

#define WASTE_TIME_PRINTING_WELCOME

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

  /*
   * using the boot cpu local information page, setup the CPU and
   * fd segment so we can use CPU specific information early on
   */
  extern u8 boot_cpu_local[];
  cpu::seginit(boot_cpu_local);

#ifdef WASTE_TIME_PRINTING_WELCOME
  printk("%s\n", chariot_welcome_start);
  printk("git revision: %s\n", GIT_REVISION);
  printk("\n");
#endif

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

static int kernel_init_task(void*);

static void kmain2(void) {
  init_idt();
  enable_sse();

  // for safety, unmap low memory (from bootstrap)
  *((u64*)p2v(read_cr3())) = 0;
  tlb_flush();

  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // panic("ded\n");

  ref<task_process> kproc0 = task_process::kproc_init();
  kproc0->create_task(kernel_init_task, PF_KTHREAD, nullptr);
  kproc0->create_task(idle_task, PF_KTHREAD, nullptr);



  KINFO("starting scheduler\n");
  sti();
  sched::beep();
  sched::run();

  // [noreturn]
}

/**
 * the kernel drops here in a kernel task
 *
 * Further initialization past getting the scheduler working should run here
 */
static int kernel_init_task(void*) {
  // TODO: initialize smp
  if (!smp::init()) panic("smp failed!\n");
  KINFO("Discovered SMP Cores\n");
  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");
  init_pit();
  KINFO("Initialized PIT\n");
  syscall_init();
  set_pit_freq(1000);

  if (fs::devfs::init() != true) {
    panic("Failed to initialize devfs\n");
  }

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_kernel_modules();
  KINFO("kernel modules initialized\n");

  // open up the disk device for the root filesystem
  auto rootdev = dev::open("ata1");
  init_rootvfs(rootdev);
  // TODO
  fs::devfs::mount();

  // setup the tmp filesystem
  vfs::mount(make_unique<fs::tmp>(), "/tmp");
  auto tmp = vfs::open("/tmp", 0);

  auto proc = task_process::lookup(0);

  // spawn init
  pid_t init = sys::spawn();

  assert(init != -1);

  if (init != -1) {
    KINFO("init pid=%d\n", init);

    const char* init_args[] = {"/bin/init", NULL};

    const char* envp[] = {NULL};

    // TODO: setup stdin, stdout, and stderr

    int res = sys::cmdpidve(init, init_args[0], init_args, envp);
    if (res != 0) {
      KERR("failed to cmdpid init process\n");
    }
  }


  // idle!
  return idle_task(NULL);
}
