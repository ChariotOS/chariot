#define __packed __attribute__((packed))
#define __align(n) __attribute__((aligned(n)))

#include <asm.h>
#include <printk.h>
#include <serial.h>
#include <types.h>

// The linker provides us with some nice boundaries for parts of the kernel
//
// The "low kern" stuff is the boundaries of the .boot segment in memory
extern char low_kern_start;
extern char low_kern_end;

// and the "high kern" stuff is for .text and beyond. Realistically, everything
// else in memory is after high_kern_end, and that is where the 'kernel heap'
// should start
extern char high_kern_start;
extern char high_kern_end;



#define P4_INDEX(addr) ((((u64)addr / 0x1000) >> 27) & 0777)
#define P3_INDEX(addr) ((((u64)addr / 0x1000) >> 18) & 0777)
#define P2_INDEX(addr) ((((u64)addr / 0x1000) >> 9) & 0777)
#define P1_INDEX(addr) ((((u64)addr / 0x1000) >> 0) & 0777)

#define PGERR ((void *)-1)
// Converts a virtual address into its physical address
//    This is some scary code lol
void *get_physaddr(void *va) {
  // cr3 contains a pointer to p4
  u64 *p4_table = (u64*)read_cr3();
  // read the p4 table to get the p3 table
  u64 p3_table = p4_table[P4_INDEX(va)];
  // check that it is active before reading
  if ((p3_table & 1) == 0) return PGERR;
  u64 p2_table = ((u64*)(p3_table & ~0xfff))[P3_INDEX(va)];
  if ((p2_table & 1) == 0) return PGERR;
  u64 p1_table = ((u64*)(p2_table & ~0xfff))[P2_INDEX(va)];
  if ((p1_table & 1) == 0) return PGERR;
  u64 pa = ((u64*)(p1_table & ~0xfff))[P1_INDEX(va)];
  return (void*)((pa & ~0xfff) | ((u64)va & 0xfff));
}

u64 strlen(const char *str) {
  const char *s;
  for (s = str; *s; ++s)
    ;
  return (s - str);
}

extern int kernel_end;

int kmain(void) {
  serial_install();
  char *addr = 0;
  while (true) {
    char *pa = get_physaddr(addr);
    printk("%p -> %p\n", addr, pa);
    addr += 4096;
  }

  // printk("lo %p : %p\n", &low_kern_start, &low_kern_end);
  // printk("hi %p : %p\n", &high_kern_start, &high_kern_end);

  while (1) halt();
  return 0;
}
