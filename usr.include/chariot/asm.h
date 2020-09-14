#ifndef __KERNEL_ASM__
#define __KERNEL_ASM__

// A set of common assembly functions

#include <math.h>

#include "types.h"

#ifndef unlikely
#define unlikely(c) __builtin_expect((c), 0)
#endif

#ifndef likely
#define likely(c) __builtin_expect((c), 1)
#endif

#define BOOTCODE __attribute__((__section__(".boot")))
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

#define FL_CF 0x00000001         // Carry Flag
#define FL_PF 0x00000004         // Parity Flag
#define FL_AF 0x00000010         // Auxiliary carry Flag
#define FL_ZF 0x00000040         // Zero Flag
#define FL_SF 0x00000080         // Sign Flag
#define FL_TF 0x00000100         // Trap Flag
#define FL_IF 0x00000200         // Interrupt Enable
#define FL_DF 0x00000400         // Direction Flag
#define FL_OF 0x00000800         // Overflow Flag
#define FL_IOPL_MASK 0x00003000  // I/O Privilege Level bitmask
#define FL_IOPL_0 0x00000000     //   IOPL == 0
#define FL_IOPL_1 0x00001000     //   IOPL == 1
#define FL_IOPL_2 0x00002000     //   IOPL == 2
#define FL_IOPL_3 0x00003000     //   IOPL == 3
#define FL_NT 0x00004000         // Nested Task
#define FL_RF 0x00010000         // Resume Flag
#define FL_VM 0x00020000         // Virtual 8086 mode
#define FL_AC 0x00040000         // Alignment Check
#define FL_VIF 0x00080000        // Virtual Interrupt Flag
#define FL_VIP 0x00100000        // Virtual Interrupt Pending
#define FL_ID 0x00200000         // ID flag

#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_KCPU 3   // kernel per-cpu data
#define SEG_UCODE 4  // user code
#define SEG_UDATA 5  // user data+stack
#define SEG_TSS 6    // this process's task state

#define DPL_KERN 0x0
#define DPL_USER 0x3  // User DPL

// Application segment type bits
#define STA_X 0x8  // Executable segment
#define STA_E 0x4  // Expand down (non-executable segments)
#define STA_C 0x4  // Conforming code segment (executable only)
#define STA_W 0x2  // Writeable (non-executable segments)
#define STA_R 0x2  // Readable (executable segments)
#define STA_A 0x1  // Accessed

// System segment type bits
#define STS_T16A 0x1  // Available 16-bit TSS
#define STS_LDT 0x2   // Local Descriptor Table
#define STS_T16B 0x3  // Busy 16-bit TSS
#define STS_CG16 0x4  // 16-bit Call Gate
#define STS_TG 0x5    // Task Gate / Coum Transmitions
#define STS_IG16 0x6  // 16-bit Interrupt Gate
#define STS_TG16 0x7  // 16-bit Trap Gate
#define STS_T32A 0x9  // Available 32-bit TSS
#define STS_T32B 0xB  // Busy 32-bit TSS
#define STS_CG32 0xC  // 32-bit Call Gate
#define STS_IG32 0xE  // 32-bit Interrupt Gate
#define STS_TG32 0xF  // 32-bit Trap Gate

// Useful constants for memory sizes
#define KB (1024LL)
#define MB (1024LL * KB)
#define GB (1024LL * MB)
#define TB (1024LL * GB)

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define for_range(var, start, end) for (auto var = start; var < (end); var++)

template <typename T, typename U>
inline constexpr T ceil_div(T a, U b) {
  static_assert(sizeof(T) == sizeof(U));
  T result = a / b;
  if ((a % b) != 0) ++result;
  return result;
}

// #define FANCY_MEM_FUNCS
//
//
// implemented in src/mem.cpp
void *memcpy(void *dst, const void *src, size_t n);

#ifndef FANCY_MEM_FUNCS

static inline void O_memset(void *buf, char c, size_t len) {
  char *m = (char *)buf;
  for (size_t i = 0; i < len; i++) m[i] = c;
}

#else

#endif

static inline int strcmp(const char *l, const char *r) {
  for (; *l == *r && *l; l++, r++)
    ;
  return *(unsigned char *)l - *(unsigned char *)r;
}


static inline size_t strlen(const char *s) {
	if (s == NULL) return 0;
  const char *a = s;
  for (; *s; s++)
    ;
  return s - a;
}

static inline void memset(void *buf, char c, size_t len) {
  u64 b = c & 0xFF;
  u64 val =
      b | b << 8 | b << 16 | b << 24 | b << 32 | b << 40 | b << 48 | b << 56;
  char *m = (char *)buf;
#define DO_COPY(T) \
  for (; i < len - sizeof(T); i += sizeof(T)) *(T *)(m + i) = val;
  int i = 0;
  DO_COPY(u64);
  DO_COPY(u32);
  for (; i < len; i++) *(m + i) = val;
#undef DO_COPY
}

#define ZERO_OUT(x) memset(&x, 0, sizeof(x))

// memmove is just copy but you clear it out
static inline void *memmove(void *dst, const void *src, size_t n) {
  for (int i = 0; i < n; i++) {
    ((char *)dst)[i] = ((char *)src)[i];
    ((char *)src)[i] = '\0';
  }
  return dst;
}

static inline i64 min(i64 a, i64 b) {
  if (a < b) return a;
  return b;
}

static inline i64 max(i64 a, i64 b) {
  if (a > b) return a;
  return b;
}

static inline u64 read_rsp(void) {
  u64 ret;
  asm volatile("mov %%rsp, %0" : "=r"(ret));
  return ret;
}

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

uint8_t inb(off_t port);
uint16_t inw(off_t port);
uint32_t inl(off_t port);
void outb(off_t port, uint8_t val);
void outw(off_t port, uint16_t val);
void outl(off_t port, uint32_t val);

static inline u32 popcnt(u64 val) {
  u64 count = 0;

  asm volatile("popcnt %0, %1" : "=a"(count) : "dN"(val));
  return count;
}

static inline void lidt(void *p, int size) {
  volatile u16 pd[5];

  pd[0] = size - 1;
  pd[1] = (u64)p;
  pd[2] = (u64)p >> 16;
  pd[3] = (u64)p >> 32;
  pd[4] = (u64)p >> 48;
  asm volatile("lidt (%0)" : : "r"(pd));
}

static inline u64 readeflags(void) {
  u64 eflags;
  asm volatile("pushf; pop %0" : "=r"(eflags));
  return eflags;
}

#endif
