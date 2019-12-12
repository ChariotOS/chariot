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
#include <fifo_buf.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <func.h>
#include <idt.h>
#include <module.h>
#include <pci.h>
#include <pctl.h>
#include <phys.h>
#include <pit.h>
#include <printk.h>
#include <process.h>
#include <ptr.h>
#include <sched.h>
#include <set.h>
#include <smp.h>
#include <string.h>
#include <task.h>
#include <types.h>
#include <util.h>
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
    KINFO("[%s] init\n", mod->name);
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

static void print_depth(int d) { for_range(i, 0, d) printk("  "); }

void print_tree(struct fs::inode* dir, int depth = 0) {
  if (dir->type == T_DIR) {
    dir->walk_direntries(
        [&](const string& name, struct fs::inode* ino) -> bool {
          if (name != "." && name != ".." &&
              name != "boot" /* it's really noisy */) {
            print_depth(depth);
            printk("%s [ino=%d, sz=%d]\n", name.get(), ino->ino, ino->size);
            if (ino->type == T_DIR) {
              print_tree(ino, depth + 1);
            }
          }
          return true;
        });
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

struct foo : public refcounted<foo> {};

static void kmain2(void) {
  init_idt();
  enable_sse();

  // for safety, unmap low memory (from bootstrap)
  *((u64*)p2v(read_cr3())) = 0;
  tlb_flush();

  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  ref<task_process> kproc0 = task_process::kproc_init();
  kproc0->create_task(kernel_init_task, PF_KTHREAD, nullptr);

  // a descrete idle task has to be added so that the scheduler always has work
  // to do. This thread is marked as idle as well as not to be promoted in the
  // mlfq scheduler
  auto idle_tid = kproc0->create_task(idle_task, PF_KTHREAD, nullptr);
  auto idle = task::lookup(idle_tid);
  idle->priority = PRIORITY_IDLE;
  idle->is_idle_thread = true;

  KINFO("starting scheduler\n");
  sti();
  sched::beep();
  sched::run();

  // [noreturn]
}

extern "C" char smp_trampoline_start[];
extern "C" char smp_trampoline_end[];

extern "C" int bios_mmap[16];

static map<off_t, string> symbols;

void parse_ksyms(size_t len, char* buf) {
  off_t off = 0;
  while (off < len) {
    off_t addr = 0;
    sscanf(buf + off, "%016lx", &addr);
    char* name = buf + off + 19 /* magic number */;

    while (buf[off] != '\n') {
      off++;
    }
    buf[off] = '\0';
    symbols[addr] = name;
    off++;
  }
  KINFO("parsed %d symbols out of the kernel\n", symbols.size());
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

  // initialize the local apic
  //    (sets up timer interupts)
  smp::lapic_init();

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_kernel_modules();
  KINFO("kernel modules initialized\n");

  // open up the disk device for the root filesystem
  auto rootdev = dev::open("ata0p1");
  init_rootvfs(rootdev);

  // TODO
  vfs::fdopen("/", 0);

  auto syms = vfs::fdopen("/boot/kernel.syms");
  struct stat s;
  syms.m_file->stat(&s);

  /*
  auto buf = kmalloc(s.st_size);
  syms.read(buf, s.st_size);
  parse_ksyms(s.st_size, (char*)buf);
  kfree(buf);
  */

  auto proc = task_process::lookup(0);
  // spawn the user process
  pid_t init = sys::pctl(0, PCTL_SPAWN, 0);
  assert(init != -1);
  KINFO("init pid=%d\n", init);
  // launch /bin/init
  const char* init_args[] = {"/bin/init", NULL};

  struct pctl_cmd_args cmdargs {
    .path = (char*)init_args[0], .argc = 1, .argv = (char**)init_args,
    // its up to init to deal with env variables. (probably reading from an
    // initial file or something)
        .envc = 0, .envv = NULL,
  };

  // TODO: setup stdin, stdout, and stderr
  int res = sys::pctl(init, PCTL_CMD, (u64)&cmdargs);
  if (res != 0) {
    KERR("failed to cmdpid init process\n");
  }

  // yield back to scheduler, we don't really want to run this thread anymore
  // TODO: run sys::wait() on the process we just spawned, and spawn it again
  while (1) {
    sched::yield();
  }

  panic("main kernel thread reached unreachable code\n");
}
