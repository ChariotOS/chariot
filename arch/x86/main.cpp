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
#include "acpi/acpi.h"
#include "cpuid.h"
#include "fpu.h"
#include "smp.h"

#include <arch.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();
extern "C" void call_with_new_stack(void *, void *);


static void kmain2(void);

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif


void dump_multiboot(uint64_t mbd) {
  debug("Multiboot 2 header dump:\n");
  mb2::foreach (mbd, [](auto *tag) {
    switch (tag->type) {
      case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
        struct multiboot_tag_string *str = (struct multiboot_tag_string *)tag;
        debug("  Boot loader: %s\n", ((struct multiboot_tag_string *)tag)->string);
        break;
      }
      case MULTIBOOT_TAG_TYPE_ELF_SECTIONS: {
        struct multiboot_tag_elf_sections *elf = (struct multiboot_tag_elf_sections *)tag;
        debug("  ELF size=%u, num=%u, entsize=%u, shndx=%u, sechdr=%p\n", elf->size, elf->num, elf->entsize, elf->shndx,
              elf->sections);
        break;
      }
      case MULTIBOOT_TAG_TYPE_MMAP: {
        debug("  Multiboot2 memory map detected\n");
        break;
      }

      case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
        debug("  Multiboot2 basic meminfo detected\n");
        break;
      }
      case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
        struct multiboot_tag_framebuffer_common *fb = (struct multiboot_tag_framebuffer_common *)tag;
        debug("  fb addr: %p, fb_width: %u, fb_height: %u\n", (void *)fb->framebuffer_addr, fb->framebuffer_width,
              fb->framebuffer_height);
        break;
      }
      case MULTIBOOT_TAG_TYPE_CMDLINE: {
        struct multiboot_tag_string *cmd = (struct multiboot_tag_string *)tag;
        debug("  Kernel Cmd line: %s\n", cmd->string);
        break;
      }

      case MULTIBOOT_TAG_TYPE_BOOTDEV: {
        struct multiboot_tag_bootdev *bd = (struct multiboot_tag_bootdev *)tag;
        debug("  Boot device: (biosdev=0x%x,slice=%u,part=%u)\n", bd->biosdev, bd->slice, bd->part);
        break;
      }
      case MULTIBOOT_TAG_TYPE_ACPI_OLD: {
        struct multiboot_tag_old_acpi *oacpi = (struct multiboot_tag_old_acpi *)tag;
        debug("  Old ACPI: rsdp=%p\n", oacpi->rsdp);
        break;
      }
      case MULTIBOOT_TAG_TYPE_ACPI_NEW: {
        struct multiboot_tag_new_acpi *nacpi = (struct multiboot_tag_new_acpi *)tag;
        debug("  New ACPI: rsdp=%p\n", nacpi->rsdp);
        break;
      }
      case MULTIBOOT_TAG_TYPE_IMAGE_BASE: {
        struct multiboot_tag_image_load_base *imb = (struct multiboot_tag_image_load_base *)tag;
        debug("  Image load base: 0x%x\n", imb->addr);
        break;
      }
      case MULTIBOOT_TAG_TYPE_MODULE: {
        struct multiboot_tag_module *mod = (struct multiboot_tag_module *)tag;
        debug("  Found module: \n");
        debug("    type:     0x%08x\n", mod->type);
        debug("    size:     0x%08x\n", mod->size);
        debug("    mod_start 0x%08x\n", mod->mod_start);
        debug("    mod_end   0x%08x\n", mod->mod_end);
        debug("    args:     %s\n", mod->cmdline);
        break;
      }
      default:
        debug("  Unhandled tag type (0x%x)\n", tag->type);
        break;
    }
  });
}




/**
 * the size of the (main cpu) scheduler stack
 */
#define STKSIZE (4096 * 2)

extern void rtc_init(void);



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


  dump_multiboot(mbd);

  kargs::init(mbd);
  smp::init();

  if (!acpi::init(mbd)) panic("acpi init failed!\n");

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



struct rsdp_descriptor {
  char Signature[8];
  uint8_t Checksum;
  char OEMID[6];
  uint8_t Revision;
  uint32_t RsdtAddress;
  uint32_t Length;
  uint64_t XsdtAddress;
  uint8_t ExtendedChecksum;
  uint8_t reserved[3];
} __attribute__((packed));


#define cpuid(in, a, b, c, d) __asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(in));


#define ACPI_EBDA_PTR_LOCATION 0x0000040E /* Physical Address */
#define ACPI_EBDA_PTR_LENGTH 2
#define ACPI_EBDA_WINDOW_SIZE 1024
#define ACPI_HI_RSDP_WINDOW_BASE 0x000E0000 /* Physical Address */
#define ACPI_HI_RSDP_WINDOW_SIZE 0x00020000
#define ACPI_RSDP_SCAN_STEP 16




int kernel_init(void *) {





  // at this point, the pit is being used for interrupts,
  // so we should go setup lapic for that
  smp::lapic_init();
  syscall_init();


  // start up the extra cpu cores
  smp::init_cores();


  pci::init(); /* initialize the PCI subsystem */
  KINFO("Initialized PCI\n");



  // walk the kernel modules and run their init function
  KINFO("Calling kernel module init functions\n");
  initialize_builtin_modules();
  KINFO("kernel modules initialized\n");




  auto root_name = kargs::get("root", "/dev/ata0p1");
  assert(root_name);



	// wait for the time to stabilize
	while (!time::stabilized()) {
		arch_halt();
	}


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



extern "C" void _hrt_start(void) {
  kmain(0, 0);
  while (1) {
  }
}

/*
#include <list_head.h>








struct wait_queue wq1;
struct wait_queue wq2;

int notifier_thread(void *) {
  while (1) {
    wq1.wake_up_all();
    sys::usleep(10000);
    wq2.wake_up_all();
    sys::usleep(10000);
  }
}


void test(void) {
  printk("\n\n====================================================================\n\n\n");



  while (1) {
    arch_disable_ints();
    arch_halt();
  }
}
*/
