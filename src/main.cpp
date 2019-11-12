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
#include <process.h>

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

static u32* buf = nullptr;

static void set_pixel(int i, int col) {
  if (i >= 0 && i < vga::width() * vga::height()) buf[i] = col;
}

static void set_pixel(int x, int y, int col) {
  set_pixel(x + y * vga::width(), col);
}

void draw_square(int x, int y, int sx, int sy, int color) {
  for_range(oy, y, y + sy) for_range(ox, x, x + sx) {
    set_pixel(ox, oy, color);
  }
}

struct info {
  int number;
  char msg[8];
};
static int screen_drawer(void*) {
  int fd = sys::open("/hello.txt", O_RDWR);

  int i = 0;
  while (0) {
    char buf[256];

    // seek to the start, read
    sys::lseek(fd, 0, SEEK_SET);
    int nread = sys::read(fd, buf, 256);

    // increment the count
    auto* info = (struct info*)buf;

    if (i == 0) {
      // printk("used to be %d, resetting...\n", info->number);
      // info->number = 0;
    }
    i++;
    info->number++;

    // reset... write
    sys::lseek(fd, 0, SEEK_SET);
    sys::write(fd, buf, nread);

    printk("%d\n", info->number);
  }

  // buf = new u32[vga::npixels()];

  auto rand = dev::open("random");

  int catfd = sys::open("/misc/cat.raw", O_RDONLY);
  if (catfd < 0) {
    printk("FAILED\n");
    return 0;
  }

  // hardcoding size, no stat syscall
  int img_size = 1228800;

  int pixelc = img_size / sizeof(u32);

  int width = 512;

  int cycle = 0;

  auto pbuf = new int[width];
  while (1) {
    for (int i = 0; i < pixelc; i += width) {
      int stat = sys::read(catfd, pbuf, width * sizeof(u32));

      if (stat < 0) {
        delete[] pbuf;
        break;
      }

      for (int o = 0; o < width; o++) {
        /*
        u32 pix = pbuf[o];
        char r = pix >> (0 + cycle);
        char g = pix >> (8 + cycle);
        char b = pix >> (16 + cycle);
        vga::set_pixel(i + o, vga::rgb(r, g, b));
        */
      }
    }

    // vga::flush_buffer(buf, vga::npixels());

    cycle += 1;
    cycle %= 4;
    sys::lseek(catfd, 0, SEEK_SET);
  }

  delete[] pbuf;

  // delete[] buf;
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
  // if (!smp::init()) panic("smp failed!\n");

  // initialize the PCI subsystem by walking the devices and creating an
  // internal representation that is faster to access later on
  pci::init();
  KINFO("Initialized PCI\n");

  // initialize the programmable interrupt timer
  init_pit();
  KINFO("Initialized PIT\n");

  syscall_init();

  // Set the PIT interrupt frequency to how many times per second it should fire
  set_pit_freq(1000);

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

  ref<task_process> kproc0 = nullptr;
  // initialize the kernel process
  {
    int kproc0_error = 0;

    string kproc0_name = "essedarius";
    vec<string> kproc_args;
    kproc_args.push(kproc0_name);
    // spawn the kernel process
    kproc0 = task_process::spawn(kproc0_name, 0, 0, -1, kproc0_error,
                                      move(kproc_args), PF_KTHREAD, 0);

    if (kproc0_error != 0) {
      panic("creating the initial kernel process failed with error code %d!\n",
            kproc0_error);
    }
    KINFO("spawned kproc0\n");
  }

  kproc0->create_task(idle_task, PF_KTHREAD /* TODO: possible idle flag? */, nullptr);

  kproc0->create_task(screen_drawer, PF_KTHREAD, nullptr);

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
