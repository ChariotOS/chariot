#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <dev/ata.h>
#include <dev/serial.h>
#include <idt.h>
#include <mem.h>
#include <module.h>
#include <paging.h>
#include <pci.h>
#include <pit.h>
#include <posix.h>
#include <printk.h>
#include <types.h>

#include <asm.h>
#include <dev/CMOS.h>
#include <dev/RTC.h>
#include <dev/mbr.h>
#include <device.h>
#include <fs/ext2.h>
#include <fs/file.h>
#include <func.h>
#include <map.h>
#include <phys.h>
#include <ptr.h>
#include <smp.h>
#include <string.h>
#include <uuid.h>
#include <vec.h>
#include <vga.h>

extern int kernel_end;

// in src/arch/x86/sse.asm
extern "C" void enable_sse();

void initialize_kernel_modules(void) {
  extern struct kernel_module_info __start__kernel_modules[];
  extern struct kernel_module_info __stop__kernel_modules[];
  struct kernel_module_info* dev = __start__kernel_modules;
  int i = 0;
  while (dev != __stop__kernel_modules) {
    dev->initfn();
    dev = &(__start__kernel_modules[++i]);
  }
}

typedef void (*func_ptr)(void);

extern "C" func_ptr __init_array_start[0], __init_array_end[0];

static void call_global_constructors(void) {
  for (func_ptr* func = __init_array_start; func != __init_array_end; func++)
    (*func)();
}

extern "C" void call_with_new_stack(void*, void*);

void do_drive_thing(u64 addr, bool master) {
  auto drive = make_ref<dev::ata>(addr, master);

  if (!drive->identify()) return;

  auto buf = kmalloc(drive->block_size());

  drive->read_block(2, (u8*)buf);


  printk("\nPIO:");

  for (int i = 0; i < drive->block_size(); i++) {
    if (i % 24 == 0) printk("\n");
    u8 c = ((u8*)buf)[i];
    printk("%02x ", c);
  }


  printk("\nDMA:");

  drive->read_block_dma(2, (u8*)buf);

  for (int i = 0; i < drive->block_size(); i++) {
    if (i % 24 == 0) printk("\n");
    u8 c = ((u8*)buf)[i];
    printk("%02x ", c);
  }

  auto fs = fs::ext2(*drive);

  if (!fs.init()) {
    printk("failed to init ext2\n");
  }
  /*
    auto mbr = dev::mbr(drive);

    if (!mbr.parse()) {
      printk("failed to parse\n");
      return;
    }

    printk("read!\n");

    printk("%d partitions\n", mbr.part_count());

    for_range(i, 0, mbr.part_count()) {
      auto part = mbr.partition(i);

      auto fs = fs::ext2(*part);

      if (!fs.init()) {
        printk("failed to parse ext2\n");
      }
    }
    */
}

string disk_name(u64 index) { return string::format("disk%d", index); }
string disk_name_part(u64 index, u64 part) {
  return string::format("disk%dp%d", index, part);
}

[[noreturn]] void kmain2(void) {

  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

  // initialize smp
  if (!smp::init()) {
    panic("smp failed!\n");
  }

  // initialize the PCI subsystem
  pci::init();

  // initialize the programmable interrupt timer
  init_pit();
  // and set the interval, or whatever
  set_pit_freq(100);

  // finally, enable interrupts
  sti();

  // now that we have a decent programming enviroment, we can go through and
  // register all the kernel modules
  // NOTE: kernel module init code should not do any work outside of registering
  //       andlers, as the enviroment they are run in is fairly minimal and have
  //       no complex things like processes or devices.
  initialize_kernel_modules();

  // do_drive_thing(0x1F0, true);

  // spin forever
  printk("\n\nno more work. spinning.\n");
  while (1) {
  }
}

extern "C" char chariot_welcome_start[];

#ifndef GIT_REVISION
#define GIT_REVISION "NO-GIT"
#endif

extern void rtc_init(void);

#define WASTE_TIME_PRINTING_WELCOME

extern "C" int kmain(u64 mbd, u64 magic) {
  rtc_init();  // initialize the clock
  // initialize the serial "driver"
  serial_install();
  vga::init();

  vga::set_color(vga::color::white, vga::color::black);
  vga::clear_screen();

#ifdef WASTE_TIME_PRINTING_WELCOME
  printk("%s\n", chariot_welcome_start);
#endif
  printk("git: %s\n", GIT_REVISION);
  printk("\n");

  init_idt();

  // now that we have interupts working, enable sse! (fpu)
  enable_sse();

  init_mem(mbd);


  init_kernel_virtual_memory();


#define STKSIZE (4096 * 8)
  void* new_stack = (void*)((u64)kmalloc(STKSIZE) + STKSIZE);


  void *new_main = p2v(kmain2);

  printk("new_main = %p\n", new_main);


  call_with_new_stack(new_stack, new_main);
  // ??
  printk("should not have gotten back here\n");

  return 0;
}

