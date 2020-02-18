#include <arch.h>
#include <asm.h>
#include <atom.h>
#include <cpu.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/blk_cache.h>
#include <dev/driver.h>
#include <dev/mbr.h>
#include <dev/serial.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <fs/vfs.h>
#include <func.h>
#include <kargs.h>
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
#include <types.h>
#include <util.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void *, void *);

static void print_depth(int d) { for_range(i, 0, d) printk("  "); }

void print_tree(struct fs::inode *dir, int depth = 0) {
  if (dir->type == T_DIR) {
    dir->walk_direntries(
        [&](const string &name, struct fs::inode *ino) -> bool {
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

struct multiboot_info *mbinfo;
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

  mbinfo = (struct multiboot_info *)(u64)p2v(mbd);
  /**
   * startup the high-kernel virtual mapping and the heap allocator
   */
  init_kernel_virtual_memory();

  void *new_stack = (void *)((u64)kmalloc(STKSIZE) + STKSIZE);

  // call the next phase main with the new allocated stack
  call_with_new_stack(new_stack, (void *)kmain2);

  // ?? - gotta loop forever here to make gcc happy. using [[gnu::noreturn]]
  // requires that it never returns...
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

static void kmain2(void) {
  irq::init();
  enable_sse();

  // for safety, unmap low memory (from boot.asm)
  *((u64 *)p2v(read_cr3())) = 0;
  arch::flush_tlb();
  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  kargs::init(mbinfo);

  // TODO: initialize smp
  if (!smp::init()) panic("smp failed!\n");
  KINFO("Discovered SMP Cores\n");

  // initialize the local apic
  //    (sets up timer interupts)
  smp::lapic_init();

  // initialize the scheduler
  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  // create the initialization thread.
  sched::proc::create_kthread(kernel_init);


  KINFO("starting scheduler\n");
  arch::sti();
  // sched::beep();
  sched::run();

  panic("sched::run() returned\n");
  // [noreturn]
}
