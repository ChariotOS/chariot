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
static int userinit_idle(void* arg) {
  // spawn init
  pid_t init = sys::spawn();

  assert(init != -1);

  if (init != -1) {
    KINFO("init pid=%d\n", init);

    const char* init_args[] = {"/bin/init", NULL};

    const char *envp[] = {NULL};

    // TODO: setup stdin, stdout, and stderr

    int res = sys::cmdpidve(init, init_args[0], init_args, envp);
    if (res != 0) {
      KERR("failed to cmdpid init process\n");
    }
  }

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

void screen_spam(int color, int cx, int cy) {
  int r = min(vga::height(), vga::width()) / 2;
  long angle = 1;

  // int cx = vga::width() / 2;
  // int cy = vga::height() / 2;

  while (1) {
    for (int R = 0; R < r; R++) {
      cpu::pushcli();

      float a = angle / 100.0;

      // double R = (r / 2) + (r / 2 * cos(a / ((float)r * 4)));

      int x = R * cos(a) + cx;
      int y = R * sin(a) + cy;

      if (x >= 0 && x < vga::width() && y >= 0 && y < vga::height())
        vga::set_pixel(x, y, color);
      // vga::set_pixel(x, y, vga::hsl(fmod(angle / 40.0, 1.0), 1, 0.5));
      angle += 314;

      cpu::popcli();
    }
  }
}

int task1(void*) {
  int w = vga::width();
  screen_spam(0xFF0000, w / 2, vga::height() / 2);
  return 0;
}

int task2(void*) {
  int w = vga::width();
  screen_spam(0x0000FF, w / 2, vga::height() / 2);
  return 0;
}

static void kmain2(void) {
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
   * unmap the low memory, as the kenrel needs to work without it in order to
   * support process level mappings later on. To do this, we just remove the
   * entry from the page table. "simple"
   *
   * this caused me a big oof, because after this call, any references
   * to low memory are invalid, which means vga framebuffers (the main problem)
   * need to now reference high kernel virtual memory
   */
  *((u64*)p2v(read_cr3())) = 0;
  tlb_flush();
  KINFO("cleared low memory 1:1 mapping\n");

  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();
  KINFO("called global constructors\n");

  // initialize smp
  if (!smp::init()) panic("smp failed!\n");
  KINFO("Discovered SMP Cores\n");

  // initialize the PCI subsystem by walking the devices and creating an
  // internal representation that is faster to access later on
  pci::init();
  KINFO("Initialized PCI\n");

  // initialize the programmable interrupt timer
  init_pit();
  KINFO("Initialized PIT\n");

  syscall_init();

  // Set the PIT interrupt frequency to how many times per second it should fire
  set_pit_freq(100);

  if (fs::devfs::init() != true) {
    panic("Failed to initialize devfs\n");
  }

  // KINFO("Detecting CPU speed\n");
  // cpu::calc_speed_khz();
  // KINFO("CPU SPEED: %fmHz\n", cpu::current().speed_khz / 1000000.0);

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_kernel_modules();
  KINFO("kernel modules initialized\n");

  // open up the disk device for the root filesystem
  auto rootdev = dev::open("ata1");

  // auto rootcache = make_ref<dev::blk_cache>(rootdev, 512);

  // setup the root vfs
  init_rootvfs(rootdev);

  // mount the devfs
  fs::devfs::mount();

  // setup the tmp filesystem
  vfs::mount(make_unique<fs::tmp>(), "/tmp");
  auto tmp = vfs::open("/tmp", 0);

  ref<task_process> kproc0 = task_process::kproc_init();
  kproc0->create_task(userinit_idle, PF_KTHREAD /* TODO: possible idle flag? */,
                      nullptr);

  // kproc0->create_task(task1, PF_KTHREAD, nullptr);
  // kproc0->create_task(task2, PF_KTHREAD, nullptr);

  /*
  {
  // allocate a page
  auto pg = p2v(phys::alloc(1));

  // open the binary
  auto bin = vfs::open("/bin/hello", 0);
  assert(bin); // make sure it exists :)
  fs::filedesc fd(bin, 0);
  // map the user page in
  paging::map(0x1000, (u64)v2p(pg), paging::pgsize::page,
              PTE_W | PTE_P | PTE_U);
  // read the binary into the location
  fd.read((void*)0x1000, bin->size());
  // create a task where rip is at that location
  kproc0->create_task((int (*)(void*))0x1000, 0, nullptr);
  }
  */

  // enable interrupts and start the scheduler
  sti();
  sched::beep();
  KINFO("starting scheduler\n");
  sched::run();

  // spin forever
  panic("should not have returned here\n");
  while (1) {
    halt();
  }
}
