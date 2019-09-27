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

static u32* buf = nullptr;
static int mouse_x = 0;
static int mouse_y = 0;
static bool clicked = false;

static int cx, cy;

static void set_pixel(int x, int y, int col) {
  if (x >= 0 && x < vga::width() && y >= 0 && y < vga::height())
    buf[x + y * vga::width()] = col;
}

void draw_square(int x, int y, int sx, int sy, int color) {
  for_range(oy, y, y + sy) for_range(ox, x, x + sx) {
    set_pixel(ox, oy, color);
  }
}

static void screen_drawer(void) {
  int c = 0;

  buf = new u32[vga::npixels()];

  int fd = sys::open("/dev/random", O_RDWR);

  printk("fd=%d\n", fd);

  auto rand = dev::open("random");




  while (1) {
    int size = 2;

    // syscall(0, c, 'a');

    rand->read(0, sizeof(size), &size);
    size = (size % 40) + 10;

    rand->read(0, sizeof(mouse_x), &mouse_x);
    rand->read(0, sizeof(mouse_y), &mouse_y);

    mouse_x %= vga::width();
    mouse_y %= vga::height();

    c++;

    int width = 25;
    u32 col = vga::hsl((c % width) / (float)width, 1, 0.5);

    if (clicked) draw_square(cx, cy, mouse_x - cx, mouse_y - cy, col);
    draw_square(mouse_x, mouse_y, size, size, col);

    if (c % 256 == 0) {
      vga::flush_buffer(buf, vga::npixels());
      // memset(buf, 0x00, vga::npixels() * sizeof(u32));
    }

    // cpu::sleep_ms(16);
  }

  delete[] buf;
}

void init_rootvfs(ref<dev::device> dev) {
  auto rootfs = make_unique<fs::ext2>(dev);
  if (!rootfs->init()) panic("failed to init ext2 on root disk\n");
  if (vfs::mount_root(move(rootfs)) < 0) panic("failed to mount rootfs");
}

atom<int> nidles = 0;
static void idle_task(void) {
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

  assert(fs::devfs::init());

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

  // setup the root vfs
  init_rootvfs(rootdev);

  // mount the devfs
  fs::devfs::mount();

  // setup the tmp filesystem
  vfs::mount(make_unique<fs::tmp>(), "/tmp");
  auto tmp = vfs::open("/tmp", 0);
  tmp->touch("foo", fs::file_type::file, 0777);
  tmp->mkdir("bar", 0777);

  // auto node = vfs::open("/", 0);
  // walk_tree(node);

  // create a simple idle task
  sched::spawn_kernel_thread("idle", idle_task,
                             {.timeslice = 1, .priority = PRIORITY_IDLE});

  sched::spawn_kernel_thread("rainbow", screen_drawer, {.timeslice = 1});

  sched::spawn_kernel_thread("kbd1", []() {
    auto kbd = dev::open("kbd");
    keyboard_packet_t pkt;
    while (1) {
      int nread = kbd->read(0, sizeof(pkt), &pkt);
      if (nread != sizeof(pkt)) continue;

      assert(nread == sizeof(pkt));
      // sched::beep();
      if (pkt.is_press()) {
        if (pkt.character == 'd') assert(false);
        sched::beep();
        // printk("%c", pkt.character);
      }
    }
  });

  /*
  sched::spawn_kernel_thread("mouse", []() {
    auto mouse = dev::open("mouse");
    mouse_packet_t pkt;
    int i = 0;
    while (1) {
      int nread = mouse->read(0, sizeof(pkt), &pkt);
      i++;
      // printk("i=%d\n", i);

      if (nread == 0) continue;
      if (nread != sizeof(pkt)) continue;

      int newx = mouse_x + pkt.dx;
      int newy = mouse_y + -pkt.dy;

      // KINFO("%d:%d\n", pkt.dx, -pkt.dy);

      if (newx >= vga::width()) newx = vga::width() - 1;
      if (newy >= vga::height()) newy = vga::height() - 1;

      if (newx >= 0 && newx < vga::width()) mouse_x = newx;
      if (newy >= 0 && newy < vga::height()) mouse_y = newy;

      bool is_clicked = (pkt.buttons & MOUSE_LEFT_CLICK) != 0;

      if (!clicked && is_clicked) {
        cx = newx;
        cy = newy;
      }
      clicked = is_clicked;
    }
  });
  */

  // enable interrupts and start the scheduler
  sti();
  sched::beep();
  sched::run();

  // spin forever
  panic("should not have returned here\n");
  while (1) {
    halt();
  }
}
