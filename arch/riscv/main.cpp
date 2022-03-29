#include <asm.h>
#include <cpu.h>
#include <dev/virtio/mmio.h>
#include <dev/driver.h>
#include <devicetree.h>
#include <fs/vfs.h>
#include <module.h>
#include <phys.h>
#include <printf.h>
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


#define LOG(...) PFXLOG(MAG "INIT", __VA_ARGS__)

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

static uint64_t ticks_to_ns(uint64_t ticks) { return (ticks * NS_PER_SEC) / CONFIG_RISCV_CLOCKS_PER_SECOND; }

static unsigned long riscv_high_acc_time_func(void) { return ticks_to_ns(read_csr(time)); }

static off_t dtb_ram_start = 0;
static size_t dtb_ram_size = 0;

extern "C" uint8_t secondary_core_startup_sbi[];
extern "C" uint64_t secondary_core_stack;
static bool second_done = false;


extern uint64_t _bss_start[];
extern uint64_t _bss_end[];

extern "C" void secondary_entry(int hartid) {
  struct rv::hart_state sc;
  sc.hartid = hartid;
  sc.kernel_sp = 0;

  rv::set_tp((rv::xsize_t)&sc);


  // its safe to store this on this stack.
  cpu::Core cpu;
  rv::get_hstate().cpu = &cpu;
  cpu::seginit(&cpu, NULL);
  cpu::current().primary = false;



  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();
  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));

  cpu::current().timekeeper = false;

  /* set the timer with sbi :) */
  sbi_set_timer(rv::get_time() + TICK_INTERVAL);

  second_done = true;

  arch_enable_ints();

  sched::run();

  while (1) {
  }
}

bool start_secondary(int i) {
  // start secondary cpus
  auto &sc = rv::get_hstate();
  if (i == sc.hartid) return false;

  // KINFO("[hart %d] Trying to start hart %d\n", sc.hartid, i);
  // allocate 2 pages for the secondary core
  secondary_core_stack = (uint64_t)malloc(CONFIG_RISCV_BOOTSTACK_SIZE * 4096);
  secondary_core_stack += CONFIG_RISCV_BOOTSTACK_SIZE * 4096;

  second_done = false;
  __sync_synchronize();

  auto ret = sbi_call(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, i, secondary_core_startup_sbi, 1);
  if (ret.error != SBI_SUCCESS) {
    return false;
  }
  while (second_done != true) {
    __sync_synchronize();
  }
  LOG("HART #%d started\n", i);

  return true;
}




class RISCVHart : public dev::CharDevice {
 public:
  using dev::CharDevice::CharDevice;

  virtual ~RISCVHart(void) {}

  virtual void init(void) {
    if (auto mmio = dev()->cast<hw::MMIODevice>()) {
      if (mmio->is_compat("riscv")) {
        auto status = mmio->get_prop_string("status");

        if (status.has_value()) {
          if (status.unwrap() == "disabled") return;
        }

        auto hartid = mmio->address();
        bind(ck::string::format("core%d", hartid));
        printf("FOUND HART %d\n", hartid);
        if (hartid != rv::get_hstate().hartid) {
          LOG("Trying to start hart %d\n", hartid);
#ifdef CONFIG_SMP
          // start the other core.
          start_secondary(hartid);
#else
          LOG("SMP Disabled. not starting hart#%d\n", hartid);
#endif
        }
      }
    }
  }
};


static dev::ProbeResult riscv_hart_probe(ck::ref<hw::Device> dev) {
  if (auto mmio = dev->cast<hw::MMIODevice>()) {
    if (mmio->is_compat("riscv")) {
      auto status = mmio->get_prop_string("status");
      if (status.has_value()) {
        if (status.unwrap() == "disabled") return dev::ProbeResult::Ignore;
      }

      return dev::ProbeResult::Attach;
    }
  }

  return dev::ProbeResult::Ignore;
};

driver_init("riscv,hart", RISCVHart, riscv_hart_probe);


spinlock x;

static int wakes = 0;
extern uint64_t _bss_start[];
extern uint64_t _bss_end[];


void main(int hartid, void *fdt) {
  // zero the BSS
  for (uint64_t *ptr = (uint64_t *)p2v(_bss_start); ptr < (uint64_t *)p2v(_bss_end); ptr++) {
    *ptr = 0;
  }

  printf_nolock("Hello, friend!\n");

  // get the information from SBI right away so we can use it early on
  sbi_early_init();


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


  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();


  off_t boot_free_start = (off_t)v2p(_kernel_end);
  off_t boot_free_end = boot_free_start + 1 * MB;
  LOG("Freeing bootup ram %llx:%llx\n", boot_free_start, boot_free_end);


  /* Tell the device tree to copy the device tree and parse it */
  dtb::parse((dtb::fdt_header *)p2v(rv::get_hstate().dtb));

  phys::free_range((void *)boot_free_start, (void *)boot_free_end);

  cpu::Core cpu;
  rv::get_hstate().cpu = &cpu;

  cpu::seginit(&cpu, NULL);

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
    LOG("dtb didn't contain a memory segment, guessing 128mb :^)\n");
    dtb_ram_size = 128 * MB;
    dtb_ram_start = boot_free_start;
  }


  off_t dtb_ram_end = dtb_ram_start + dtb_ram_size;
  dtb_ram_start = max(dtb_ram_start, boot_free_end + 4096);
  if (dtb_ram_end - dtb_ram_start > 0) {
    LOG("Freeing discovered ram %llx:%llx\n", dtb_ram_start, dtb_ram_end);
    phys::free_range((void *)dtb_ram_start, (void *)dtb_ram_end);
  }

  /* Now that we have a memory allocator, call global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++)
    (*func)();

  cpu::current().id = rv::get_hstate().hartid;
  cpu::current().primary = true;

  arch_enable_ints();
  sbi_init();

  dtb::promote();

  /* set the timer with sbi :) */
  sbi_set_timer(rv::get_time() + TICK_INTERVAL);


  time::set_cps(CONFIG_RISCV_CLOCKS_PER_SECOND);
  time::set_high_accuracy_time_fn(riscv_high_acc_time_func);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));
  cpu::current().timekeeper = true;

  assert(sched::init());
  LOG("Initialized the scheduler with %llu pages of ram (%llu bytes)\n", phys::nfree(), phys::bytes_free());




  sched::proc::create_kthread("main task", [](void *) -> int {
    // Init the virtual filesystem and mount a tmpfs and devfs to / and /dev
    vfs::init_boot_filesystem();

    LOG("Calling kernel module init functions\n");
    initialize_builtin_modules();
    LOG("kernel modules initialized\n");



#ifdef CONFIG_ENABLE_USERSPACE


    LOG("Mounting root filesystem\n");
    int mnt_res = vfs::mount("/dev/disk0p1", "/root", "ext2", 0, NULL);
    if (mnt_res != 0) {
      panic("failed to mount root. Error=%d\n", -mnt_res);
    }

    LOG("Bootup complete. It is now safe to move about the cabin.\n");

    auto kproc = sched::proc::kproc();
    kproc->root = vfs::get_root();
    kproc->cwd = vfs::get_root();

    ck::string init_paths = "/bin/init,/init";
    auto paths = init_paths.split(',');
    auto init_pid = sched::proc::spawn_init(paths);

    sys::waitpid(init_pid, NULL, 0);
    panic("INIT DIED!\n");

#else

		kshell::run();

#endif

    return 0;
  });


  KINFO("starting scheduler\n");
  sched::run();

  while (1)
    arch_halt();
}



extern "C" rv::xsize_t sip_bench();
ksh_def("sip-bench", "benchmark access to the sip register") {
  size_t trials = 1000;
  auto measurements = new uint64_t[trials];


  // auto start = rv::get_cycle();
  for (int i = 0; i < trials; i++) {
    measurements[i] = sip_bench();
  }
  // auto end = rv::get_cycle();

  uint64_t sum = 0;
  for (int i = 0; i < trials; i++) {
    sum += measurements[i];
  }
  uint64_t avg = sum / trials;

  printf("total: %llu cycles\n", sum / trials);
  printf("average access latency: %llu cycles\n", avg);
  // printf("wrapped access latency: %llu cycles\n", (end - start) / trials);
  /*
printf("Average access latency to SIP CSR: %llu cycles, %lluns. 1000 trials took %llu, %lluns\n", avg, ticks_to_ns(avg), sum,
    ticks_to_ns(sum));
                                  */


  delete[] measurements;
  return 0;
}
