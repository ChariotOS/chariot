#ifndef __KERNEL_ASM__
#define __KERNEL_ASM__

// A set of common assembly functions

#include "types.h"

#define BOOTCODE __attribute__ ((__section__(".boot")))
#define __packed __attribute__((packed))


#define RFLAGS_CF (1 << 0)
#define RFLAGS_PF (1 << 2)
#define RFLAGS_AF (1 << 4)
#define RFLAGS_ZF (1 << 6)
#define RFLAGS_SF (1 << 7)
#define RFLAGS_TF (1 << 8)
#define RFLAGS_IF (1 << 9)
#define RFLAGS_DF (1 << 10)
#define RFLAGS_OF (1 << 11)
#define RFLAGS_IOPL (3 << 12)
#define RFLAGS_VM ((1 << 17) | RFLAGS_IOPL)
#define RFLAGS_VIF (1 << 19)
#define RFLAGS_VIP (1 << 20)

#define CR0_PE 1
#define CR0_MP 2
#define CR0_EM (1 << 2)
#define CR0_TS (1 << 3)
#define CR0_NE (1 << 5)
#define CR0_WP (1 << 16)
#define CR0_AM (1 << 18)
#define CR0_NW (1 << 29)
#define CR0_CD (1 << 30)
#define CR0_PG (1 << 31)

#define CR4_VME 1
#define CR4_PVI 2
#define CR4_TSD (1 << 2)
#define CR4_DE (1 << 3)
#define CR4_PSE (1 << 4)
#define CR4_PAE (1 << 5)
#define CR4_MCE (1 << 6)
#define CR4_PGE (1 << 7)
#define CR4_PCE (1 << 8)
#define CR4_OSFXSR (1 << 9)
#define CR4_OSXMMEXCPT (1 << 10)
#define CR4_VMXE (1 << 13)
#define CR4_XMXE (1 << 14)
#define CR4_FSGSBASE (1 << 16)
#define CR4_PCIDE (1 << 17)
#define CR4_OSXSAVE (1 << 18)
#define CR4_SMEP (1 << 20)



static inline void memset(void *buf, char c, size_t len) {
  char *m = (char*)buf;
  for (int i = 0; i < len; i++) m[i] = c;
}


// a struct that holds all the registers
struct regs_s {
  u64 r15;
  u64 r14;
  u64 r13;
  u64 r12;
  u64 r11;
  u64 r10;
  u64 r9;
  u64 r8;
  u64 rbp;
  u64 rdi;
  u64 rsi;
  u64 rdx;
  u64 rcx;
  u64 rbx;
  u64 rax;
  u64 vector;
  u64 err_code;
  u64 rip;
  u64 cs;
  u64 rflags;
  u64 rsp;
  u64 ss;
};
typedef struct regs_s regs_t;


#define PAUSE_WHILE(x)     \
  while ((x)) {            \
    asm volatile("pause"); \
  }

#define BARRIER_WHILE(x) \
  while ((x)) {}

static inline u64 read_cr0(void) {
  u64 ret;
  asm volatile("mov %%cr0, %0" : "=r"(ret));
  return ret;
}

static inline void write_cr0(u64 data) {
  asm volatile("mov %0, %%cr0" : : "r"(data));
}

static inline u64 read_cr2(void) {
  u64 ret;
  asm volatile("mov %%cr2, %0" : "=r"(ret));
  return ret;
}

static inline void write_cr2(u64 data) {
  asm volatile("mov %0, %%cr2" : : "r"(data));
}

static inline u64 read_cr3(void) {
  u64 ret;
  asm volatile("mov %%cr3, %0" : "=r"(ret));
  return ret;
}

static inline void write_cr3(u64 data) {
  asm volatile("mov %0, %%cr3" : : "r"(data));
}

static inline u64 read_cr4(void) {
  u64 ret;
  asm volatile("mov %%cr4, %0" : "=r"(ret));
  return ret;
}

static inline void write_cr4(u64 data) {
  asm volatile("mov %0, %%cr4" : : "r"(data));
}

static inline u64 read_cr8(void) {
  u64 ret;
  asm volatile("mov %%cr8, %0" : "=r"(ret));
  return ret;
}

static inline void write_cr8(u64 data) {
  asm volatile("mov %0, %%cr8" : : "r"(data));
}

static inline uint8_t inb(uint16_t port) {
  uint8_t ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static inline uint16_t inw(uint16_t port) {
  uint16_t ret;
  asm volatile("inw %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static inline uint32_t inl(uint16_t port) {
  uint32_t ret;
  asm volatile("inl %1, %0" : "=a"(ret) : "dN"(port));
  return ret;
}

static inline void outb(uint8_t val, uint16_t port) {
  asm volatile("outb %0, %1" ::"a"(val), "dN"(port));
}

static inline void outw(uint16_t val, uint16_t port) {
  asm volatile("outw %0, %1" ::"a"(val), "dN"(port));
}

static inline void outl(uint32_t val, uint16_t port) {
  asm volatile("outl %0, %1" ::"a"(val), "dN"(port));
}

static inline void sti(void) { asm volatile("sti" : : : "memory"); }

static inline void cli(void) { asm volatile("cli" : : : "memory"); }

static inline uint64_t __attribute__((always_inline)) rdtsc(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

static inline uint64_t rdtscp(void) {
  uint32_t lo, hi;
  asm volatile("rdtscp" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}


static inline uint64_t read_rflags(void) {
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}

static inline void halt(void) { asm volatile("hlt"); }

static inline void invlpg(unsigned long addr) {
  asm volatile("invlpg (%0)" ::"r"(addr) : "memory");
}

static inline void wbinvd(void) { asm volatile("wbinvd" : : : "memory"); }

static inline void clflush(void *ptr) {
  __asm__ __volatile__("clflush (%0); " : : "r"(ptr) : "memory");
}

static inline void clflush_unaligned(void *ptr, int size) {
  clflush(ptr);
  if ((addr_t)ptr % size) {
    // ptr is misaligned, so be paranoid since we
    // may be spanning a cache line
    clflush((void *)((addr_t)ptr + size - 1));
  }
}

/**
 * Flush all non-global entries in the calling CPU's TLB.
 *
 * Flushing non-global entries is the common-case since user-space
 * does not use global pages (i.e., pages mapped at the same virtual
 * address in *all* processes).
 *
 */
static inline void tlb_flush(void) {
  uint64_t tmpreg;

  asm volatile(
      "movq %%cr3, %0;  # flush TLB \n"
      "movq %0, %%cr3;              \n"
      : "=r"(tmpreg)::"memory");
}

#endif
