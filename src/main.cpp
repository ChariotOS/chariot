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
#include <phys.h>
#include <ptr.h>
#include <string.h>
#include <vec.h>
#include <vga.h>
#include <map.h>

extern int kernel_end;

u64 fib(u64 n) {
  if (n < 2) return 1;

  return fib(n - 1) + fib(n - 2);
}

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
}

extern "C" u8 initrd_start[];
extern "C" u8 initrd_end[];

struct tar_header {
  char filename[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag[1];
};

// idk what this does
unsigned int getsize(const char* in) {
  unsigned int size = 0;
  unsigned int j;
  unsigned int count = 1;

  for (j = 11; j > 0; j--, count *= 8) size += ((in[j - 1] - '0') * count);

  return size;
}
static void parse_initrd(void) {

  vec<tar_header *> headers;
  auto address = (u64)initrd_start;

  for (u32 i = 0;; i++) {
    struct tar_header* header = (struct tar_header*)address;
    if (header->filename[0] == '\0') break;
    unsigned int size = getsize(header->size);
    headers.push(header);
    address += ((size / 512) + 1) * 512;
    if (size % 512) address += 512;
  }


  for (auto &h : headers) {
    printk("%s\n", h->filename);
  }
}

[[noreturn]] void kmain2(void) {
  // now that we have a stable memory manager, call the C++ global constructors
  call_global_constructors();

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

  map<int, int> m;

  m[0] = 1;
  m[1] = 2;

  printk("%d %d\n", m[0], m[1]);

  // parse_initrd();

  /*
  for (u16 addr = 0x1F0; addr <= 0x1F7; addr++) {
    do_drive_thing(addr, true);
    do_drive_thing(addr, false);
  }
  */

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

  call_with_new_stack(new_stack, p2v(kmain2));
  printk("should not have gotten back here\n");

  return 0;
}

