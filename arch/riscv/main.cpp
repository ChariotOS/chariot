#include <asm.h>
#include <cpu.h>
#include <dev/virtio_mmio.h>
#include <devicetree.h>
#include <fs/vfs.h>
#include <module.h>
#include <phys.h>
#include <printk.h>
#include <riscv/arch.h>
#include <riscv/dis.h>
#include <riscv/paging.h>
#include <riscv/plic.h>
#include <riscv/uart.h>
#include <sleep.h>
#include <syscall.h>
#include <time.h>
#include <util.h>

#define PAGING_IMPL_BOOTCODE
#include "paging_impl.h"

typedef void (*func_ptr)(void);
extern "C" func_ptr __init_array_start[0], __init_array_end[0];
extern "C" char _kernel_end[];

extern "C" char _stack_start[];
extern "C" char _stack[];

volatile long count = 0;

/* lowlevel.S, calls kerneltrap() */
extern "C" void kernelvec(void);

extern int uart_count;

#define NBITS(N) ((1LL << (N)) - 1)

void print_va(rv::xsize_t va, int entry_width, int count) {
  int mask = (1LLU << entry_width) - 1;
  int awidth = (entry_width * count) + 12;
  printk("0x%0*llx: ", awidth / 4, va & NBITS(awidth));
  for (int i = count - 1; i >= 0; i--) {
    printk("%3d ", (va >> (entry_width * i + 12)) & mask);
    // printk("%3d ", (va >> (entry_width * i + 12)) & mask);
  }
  printk("+ %012b\n", va & 0xFFF);
}



static unsigned long riscv_high_acc_time_func(void) {
  return (read_csr(time) * NS_PER_SEC) / CONFIG_RISCV_CLOCKS_PER_SECOND;
}

void main() {
  /*
   * Machine mode passes us the scratch structure through
   * the thread pointer register. We need to then move it
   * to our sscratch register
   */
  struct rv::scratch *sc = (rv::scratch *)rv::get_tp();
  /* The scratch register is a physical address. This is so the timervec doesn't have to
   * do any address translation or whatnot. We just pay the cost everywhere else! :^) */
  rv::set_sscratch((rv::xsize_t)sc);

  int hartid = rv::hartid();
  /* TODO: release these somehow :) */
  if (rv::hartid() != 0)
    while (1) arch_halt();

  /* Initialize the platform level interrupt controller for this HART */
  rv::plic::hart_init();

  rv::uart_init();

  write_csr(stvec, kernelvec);

  rv::intr_on();


  dtb::parse((dtb::fdt_header *)p2v(rv::get_scratch().dtb));

  printk(KERN_DEBUG "Freeing %dMB of ram %llx:%llx\n", CONFIG_RISCV_RAM_MB, _kernel_end - CONFIG_KERNEL_VIRTUAL_BASE,
         PHYSTOP);

  use_kernel_vm = 1;
  phys::free_range((void *)(_kernel_end - CONFIG_KERNEL_VIRTUAL_BASE), (void *)PHYSTOP);

  cpu::seginit(NULL);

  /* Now that we have a memory allocator, call global constructors */
  for (func_ptr *func = __init_array_start; func != __init_array_end; func++) {
    (*func)();
  }

  time::set_cps(CONFIG_RISCV_CLOCKS_PER_SECOND);
  time::set_high_accuracy_time_fn(riscv_high_acc_time_func);




  assert(sched::init());
  KINFO("Initialized the scheduler\n");

  cpus[0].timekeeper = true;

  sched::proc::create_kthread("test task", [](void *) -> int {
    KINFO("Calling kernel module init functions\n");
    initialize_builtin_modules();
    KINFO("kernel modules initialized\n");

    /* TODO: use device tree? */
    virtio::check_mmio(0x10001000);
    virtio::check_mmio(0x10002000);
    virtio::check_mmio(0x10003000);
    virtio::check_mmio(0x10004000);
    virtio::check_mmio(0x10005000);
    virtio::check_mmio(0x10006000);
    virtio::check_mmio(0x10007000);
    virtio::check_mmio(0x10008000);

    int mnt_res = vfs::mount("/dev/disk0p1", "/", "ext2", 0, NULL);
    if (mnt_res != 0) {
      panic("failed to mount root. Error=%d\n", -mnt_res);
    }


    /* Mount /dev and /tmp */
    vfs::mount("none", "/dev", "devfs", 0, NULL);
    vfs::mount("none", "/tmp", "tmpfs", 0, NULL);

    auto kproc = sched::proc::kproc();
    kproc->root = fs::inode::acquire(vfs::get_root());
    kproc->cwd = fs::inode::acquire(vfs::get_root());


    string init_paths = "/bin/init,/init";
    auto paths = init_paths.split(',');
    pid_t init_pid = sched::proc::spawn_init(paths);
    printk("init pid: %d\n", init_pid);





    if (0) {
      char *buf = (char *)malloc(4096);
      for (int i = 0; i < 10; i++) {
        {
          auto begin = time::now_ms();
          auto file = vfs::fdopen("/usr/res/misc/lorem.txt", O_RDONLY, 0);

          if (!file) {
            panic("no file!\n");
          }
          auto res = file.read((void *)buf, 4096);
          file.seek(0, SEEK_SET);
          printk("%db took %dms    %dB free\n", res, time::now_ms() - begin, phys::nfree() * 4096);
        }

        block::reclaim_memory();
      }
      free((void *)buf);
    }


    auto begin = time::now_ms();
    while (1) {
      // printk("us: %llu\n", time::now_us() - begin);

      begin = time::now_us();
      do_usleep(1000 * 100);
    }
    return 0;
  });


  KINFO("starting scheduler\n");
  sched::run();

  while (1) {
    arch_halt();
  }
}

