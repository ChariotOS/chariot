#include <asm.h>
#include <cpu.h>
#include <dev/driver.h>
#include <dev/virtio/mmio.h>
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

#include "sched.h"
#include <ioctl.h>
#include <rbtree.h>
#include <rbtree_augmented.h>

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

  // Set the thread pointer for thread local access
  rv::set_tp((rv::xsize_t)&sc);

  // Register this core w/ the CPU subsystem
  cpu::Core cpu;
  rv::get_hstate().cpu = &cpu;
  cpu::seginit(&cpu, NULL);
  cpu::current().primary = false;

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();
  /* Set the supervisor trap vector location */
  write_csr(stvec, kernelvec);
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable
   * floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));

  cpu::current().timekeeper = false;

  /* set the timer with sbi :) */
  sbi_set_timer(rv::get_time() + TICK_INTERVAL);

  second_done = true;

  arch_enable_ints();
  assert(sched::init());

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

static void reverse_string(char *begin, char *end) {
  do {
    char a = *begin, b = *end;
    *end = a;
    *begin = b;
    begin++;
    end--;
  } while (begin < end);
}

unsigned convert_utf8(unsigned int c, char *utf8) {
  int bytes = 1;

  *utf8 = c;
  if (c > 0x7f) {
    int prefix = 0x40;
    char *p = utf8;
    do {
      *p++ = 0x80 + (c & 0x3f);
      bytes++;
      prefix >>= 1;
      c >>= 6;
    } while (c > prefix);
    *p = c - 2 * prefix;
    reverse_string(utf8, p);
  }
  return bytes;
}

void print_utf16(uint16_t c) {
  char buf[16];
  unsigned len = convert_utf8(c, buf);
  buf[len + 1] = 0;
  printf("%s", buf);
}

spinlock x;

static int wakes = 0;
extern uint64_t _bss_start[];
extern uint64_t _bss_end[];
extern rv::xsize_t kernel_page_table[4096 / sizeof(rv::xsize_t)];

template <typename Fn>
void display_braile_bitmap(int width, int height, Fn cb) {
  static const wchar_t *chars =
      L"⠀⠁⠂⠃⠄⠅⠆⠇⡀⡁⡂⡃⡄⡅⡆⡇⠈⠉⠊⠋⠌⠍⠎⠏⡈⡉⡊⡋⡌⡍⡎⡏⠐⠑⠒⠓⠔⠕⠖⠗⡐⡑⡒⡓⡔⡕⡖⡗⠘⠙⠚⠛⠜⠝⠞⠟⡘⡙⡚⡛⡜⡝⡞⡟⠠⠡⠢⠣⠤⠥⠦"
      L"⠧⡠⡡⡢⡣⡤⡥⡦⡧⠨⠩⠪⠫⠬⠭⠮⠯⡨⡩⡪⡫⡬⡭⡮⡯⠰⠱⠲⠳⠴⠵⠶⠷⡰⡱⡲⡳⡴⡵⡶⡷⠸⠹⠺⠻⠼⠽⠾⠿⡸⡹⡺⡻⡼⡽⡾⡿⢀⢁⢂"
      L"⢃⢄⢅⢆⢇⣀⣁⣂⣃⣄⣅⣆⣇⢈⢉⢊⢋⢌⢍⢎⢏⣈⣉⣊⣋⣌⣍⣎⣏⢐⢑⢒⢓⢔⢕⢖⢗⣐⣑⣒⣓⣔⣕⣖⣗⢘⢙⢚⢛⢜⢝⢞⢟⣘⣙⣚⣛⣜⣝⣞⣟⢠⢡⢢⢣⢤⢥⢦⢧⣠⣡"
      L"⣢⣣⣤⣥⣦⣧⢨⢩⢪⢫⢬⢭⢮⢯⣨⣩⣪⣫⣬⣭⣮⣯⢰⢱⢲⢳⢴⢵⢶⢷⣰⣱⣲⣳⣴⣵⣶⣷⢸⢹⢺⢻⢼⢽⢾⢿⣸⣹⣺⣻⣼⣽⣾⣿";

  constexpr int CHAR_WIDTH = 2;
  constexpr int CHAR_HEIGHT = 4;
  const int height_chars = height / CHAR_HEIGHT;
  const int width_chars = width / CHAR_WIDTH;
  for (int y = 0; y < height_chars; y++) {
    for (int x = 0; x < width_chars; x++) {
      int baseX = x * CHAR_WIDTH;
      int baseY = y * CHAR_HEIGHT;
      int charIndex = 0;
      int value = 1;
      for (int charX = 0; charX < CHAR_WIDTH; charX++) {
        for (int charY = 0; charY < CHAR_HEIGHT; charY++) {
          const int bitmapX = baseX + charX;
          const int bitmapY = baseY + charY;
          const bool pixelExists = bitmapX < width && bitmapY < height;
          if (pixelExists && cb(bitmapX, bitmapY)) {
            charIndex += value;
          }
          value *= 2;
        }
      }
      print_utf16(chars[charIndex]);
    }
    printf("\n");
  }
}

static spinlock logger_lock;
static long logger_count = 0;
static const char *last_logger = NULL;
static int logger(void *arg) {
  // const char *s = (const char *)curthd->name.get();
  while (1) {
    sched::yield();
  }
  return 0;
}

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
  // rv::plic::hart_init();

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
  /* set SUM bit in sstatus so kernel can access userspace pages. Also enable
   * floating point */
  write_csr(sstatus, read_csr(sstatus) | (1 << 18) | (1 << 13));
  cpu::current().timekeeper = true;

  // discover the plic on the system, and then initialize this hart's state on
  // it
  rv::plic::discover();
  rv::plic::hart_init();

  assert(sched::init());
  LOG("Initialized the scheduler with %llu pages of ram (%llu bytes)\n", phys::nfree(), phys::bytes_free());

  /*
for (int i = 0; i < 20; i++) {
char name[20];
sprintf(name, "%d", i);
sched::proc::create_kthread(name, [](void *) -> int {
while (1) {
  sched::yield();
}
return 0;
});
}
  */
  // while(1) {}
  // sched::run();

  sched::proc::create_kthread("[kmain]", [](void *) -> int {
    // Init the virtual filesystem and mount a tmpfs and devfs to / and /dev
    vfs::init_boot_filesystem();

    LOG("Calling kernel module init functions\n");
    initialize_builtin_modules();
    LOG("kernel modules initialized\n");

    // Now that we are definitely in the high half and all cores have been
    // booted, nuke the lower half of the kernel page table for sanity reasons
    const rv::xsize_t nents = 4096 / sizeof(rv::xsize_t);
    const rv::xsize_t half = nents / 2;
    for (int i = 0; i < half; i++) {
      kernel_page_table[i] = 0;
    }
    rv::sfence_vma();

    sched::proc::create_kthread("[reaper]", Process::reaper);

#ifdef CONFIG_ENABLE_USERSPACE

    LOG("Mounting root filesystem\n");
    int mnt_res = vfs::mount("/dev/disk0p1", "/root", "ext2", 0, NULL);
    if (mnt_res != 0) {
      panic("failed to mount root. Error=%d\n", -mnt_res);
    }

    int chroot_res = vfs::chroot("/root");
    if (chroot_res != 0) {
      panic("Failed to chroot into /root. Error=%d\n", -mnt_res);
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

  delete[] measurements;
  return 0;
}
