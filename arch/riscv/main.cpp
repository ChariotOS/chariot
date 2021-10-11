#include <asm.h>
#include <cpu.h>
#include <dev/virtio/mmio.h>
#include <devicetree.h>
#include <fs/vfs.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/paging.h>
#include <riscv/plic.h>
#include <riscv/sbi.h>
#include <riscv/uart.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <util.h>

#include <rbtree.h>
#include <rbtree_augmented.h>
#include <ioctl.h>

#define PAGING_IMPL_BOOTCODE
#include "paging_impl.h"

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" char _kernel_end[];


extern "C" char _initrd_start[];
extern "C" char _initrd_end[];


/* lowlevel.S, calls kerneltrap() */
extern "C" void kernelvec(void);


struct cpio_hdr {
  unsigned short magic;
  unsigned short dev;
  unsigned short ino;
  unsigned short mode;
  unsigned short uid;
  unsigned short gid;
  unsigned short nlink;
  unsigned short rdev;
  unsigned int mtime;
  unsigned short namesize;
  unsigned int filesize;
  char name[0];
} __attribute__((packed));



static uint16_t bswap_16(uint16_t __x) { return __x << 8 | __x >> 8; }
static uint32_t bswap_32(uint32_t __x) { return __x >> 24 | (__x >> 8 & 0xff00) | (__x << 8 & 0xff0000) | __x << 24; }

void initrd_dump(void *vbuf, size_t size) { hexdump(vbuf, size, true); }

static unsigned long riscv_high_acc_time_func(void) { return (read_csr(time) * NS_PER_SEC) / CONFIG_RISCV_CLOCKS_PER_SECOND; }

static off_t dtb_ram_start = 0;
static size_t dtb_ram_size = 0;

extern "C" uint8_t secondary_core_startup_sbi[];
extern "C" uint64_t secondary_core_stack;
static bool second_done = false;

extern "C" void secondary_entry(int hartid) {
  struct rv::hart_state sc;
  sc.hartid = hartid;
  sc.kernel_sp = 0;

  rv::set_tp((rv::xsize_t)&sc);
  cpu::seginit(NULL);
  cpu::current().primary = false;

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();
  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));
  cpus[rv::hartid()].timekeeper = false;

  /* set the timer with sbi :) */
  sbi_set_timer(rv::get_time() + TICK_INTERVAL);

  second_done = true;

  arch_enable_ints();

  sched::run();

  while (1) {
  }
}

int start_secondary(void) {
  // start secondary cpus
  for (int i = 0; i < CONFIG_MAX_CPUS; i++) {
    // allocate 2 pages for the secondary core
    secondary_core_stack = (uint64_t)malloc(CONFIG_RISCV_BOOTSTACK_SIZE * 4096);
    secondary_core_stack += CONFIG_RISCV_BOOTSTACK_SIZE * 4096;

    second_done = false;
    __sync_synchronize();


    auto ret = sbi_call(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, i, secondary_core_startup_sbi, 0);
    if (ret.error != SBI_SUCCESS) {
      continue;
    }

    while (second_done != true) {
      __sync_synchronize();
    }
    printk(KERN_INFO "HART #%d started\n", i);
  }

  return 0;
}


#if 0
template <typename T>
struct lref {
 public:
  lref(T &val) : val(val) {
    val.lock();
  }

  ~lref(void) {
    val.unlock();
  }

  T *operator->(void) {
    return &val;
  }

  T *operator*(void) {
    return &val;
  }

 private:
  T &val;
};

struct lockable {
  void do_thing(void) {
    printk("do thing\n");
  }

  void lock(void) {
    printk("lock\n");
  }
  void unlock(void) {
    printk("unlock\n");
  }
};

static lockable *g_lockable = NULL;
auto get_lockable(void) {
  if (g_lockable == NULL) g_lockable = new lockable;
  return lref(*g_lockable);
}
#endif


static int wakes = 0;
void main(int hartid, void *fdt) {
#ifdef CONFIG_SBI
  // get the information from SBI right away so we can use it early on
  sbi_early_init();
#endif

  /*
   * Machine mode passes us the scratch structure through
   * the thread pointer register. We need to then move it
   * to our sscratch register after copying it
   */
  struct rv::hart_state *sc = (rv::hart_state *)p2v(rv::get_tp());
  sc->hartid = hartid;
  sc->dtb = (dtb::fdt_header *)fdt;
  rv::set_tp((rv::xsize_t)p2v(sc));

  rv::get_hstate().kernel_sp = 0;

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();

  rv::uart_init();

  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);


  off_t boot_free_start = (off_t)v2p(_kernel_end);
  off_t boot_free_end = boot_free_start + 1 * MB;
  printk(KERN_DEBUG "Freeing bootup ram %llx:%llx\n", boot_free_start, boot_free_end);



  /* Tell the device tree to copy the device tree and parse it */
  dtb::parse((dtb::fdt_header *)p2v(rv::get_hstate().dtb));

  phys::free_range((void *)boot_free_start, (void *)boot_free_end);

  cpu::seginit(NULL);

  dtb::walk_devices([](dtb::node *node) -> bool {
    if (!strcmp(node->name, "memory")) {
      /* We found the ram (there might be more, but idk for now) */
      dtb_ram_size = node->reg.length;
      dtb_ram_start = node->reg.address;
      return false;
    }
    return true;
  });


  if (dtb_ram_start == 0) {
    printk(KERN_ERROR "dtb didn't contain a memory segment, guessing 128mb :^)\n");
    dtb_ram_size = 128 * MB;
    dtb_ram_start = boot_free_start;
  }



  off_t dtb_ram_end = dtb_ram_start + dtb_ram_size;
  dtb_ram_start = max(dtb_ram_start, boot_free_end + 4096);
  if (dtb_ram_end - dtb_ram_start > 0) {
    printk(KERN_DEBUG "Freeing discovered ram %llx:%llx\n", dtb_ram_start, dtb_ram_end);
    phys::free_range((void *)dtb_ram_start, (void *)dtb_ram_end);
  }

  /* Now that we have a memory allocator, call global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++)
    (*func)();


  cpu::current().primary = true;

  arch_enable_ints();

#ifdef CONFIG_SBI
  sbi_init();
  /* set the timer with sbi :) */
  sbi_set_timer(rv::get_time() + TICK_INTERVAL);
#endif


  time::set_cps(CONFIG_RISCV_CLOCKS_PER_SECOND);
  time::set_high_accuracy_time_fn(riscv_high_acc_time_func);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));

  cpus[rv::hartid()].timekeeper = true;

  assert(sched::init());
  KINFO("Initialized the scheduler with %llu pages of ram (%llu bytes)\n", phys::nfree(), phys::bytes_free());



  sched::proc::create_kthread("main task", [](void *) -> int {
    KINFO("Calling kernel module init functions\n");
    initialize_builtin_modules();
    KINFO("kernel modules initialized\n");


    dtb::walk_devices([](dtb::node *node) -> bool {
      if (!strcmp(node->compatible, "virtio,mmio")) {
        virtio::check_mmio((void *)node->address, node->irq);
      }
      return true;
    });

    start_secondary();

    int mnt_res = vfs::mount("/dev/disk0p1", "/", "ext2", 0, NULL);
    if (mnt_res != 0) {
      panic("failed to mount root. Error=%d\n", -mnt_res);
    }



    KINFO("Bootup complete. It is now safe to move about the cabin.\n");

#ifdef CONFIG_ENABLE_USERSPACE

    auto kproc = sched::proc::kproc();
    kproc->root = fs::inode::acquire(vfs::get_root());
    kproc->cwd = fs::inode::acquire(vfs::get_root());

    ck::string init_paths = "/bin/init,/init";
    auto paths = init_paths.split(',');
    pid_t init_pid = sched::proc::spawn_init(paths);

    sys::waitpid(init_pid, NULL, 0);
    panic("INIT DIED!\n");

#endif

    return 0;
  });


  KINFO("starting scheduler\n");
  sched::run();

  while (1)
    arch_halt();
}
