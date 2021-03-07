/* higher-half virtual memory address */
#define KERNEL_VMA 0xffff880000000000

/* MSR numbers */
#define MSR_EFER 0xC0000080

/* EFER bitmasks */
#define EFER_LM 0x100
#define EFER_NX 0x800
#define EFER_SCE 0x001

/* CR0 bitmasks */
#define CR0_PAGING 0x80000000

/* CR4 bitmasks */
#define CR4_PAE 0x20
#define CR4_PSE 0x10

/* page flag bitmasks */
#define PG_PRESENT 0x1
#define PG_WRITABLE 0x2
#define PG_USER 0x4
#define PG_BIG 0x80
#define PG_NO_EXEC 0x8000000000000000

/* page and table size constants */
#define LOG_TABLE_SIZE 9
#define LOG_PAGE_SIZE 12
#define PAGE_SIZE (1 << LOG_PAGE_SIZE)
#define TABLE_SIZE (1 << LOG_TABLE_SIZE)

/* bootstrap stack size and alignment */
#define STACK_SIZE 0x1000
#define STACK_ALIGN 16

#define ENTRY(x)  \
  .globl x;       \
  .align 4, 0x90; \
  x:

#define GLOBAL(x) \
  .globl x;       \
  x:

#define END(x) .size x, .- x
