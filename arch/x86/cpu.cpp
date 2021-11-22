
#include <cpu.h>
#include <mem.h>
#include <phys.h>
#include <thread.h>
#include <process.h>
#include <mm.h>
#include <module.h>

#include <x86/msr.h>
#include <x86/smp.h>


extern "C" struct processor_state *__get_cpu_struct(void);
extern "C" void __set_cpu_struct(struct processor_state *);

extern "C" void wrmsr(u32 msr, u64 val);

struct gdt_desc64 {
  uint16_t limit;
  uint64_t base;
} __packed;
static inline void lgdt(void *data, int size) {
  gdt_desc64 gdt;
  gdt.limit = size - 1;
  gdt.base = (u64)data;

  asm volatile("lgdt %0" ::"m"(gdt));
}
static inline void ltr(u16 sel) { asm volatile("ltr %0" : : "r"(sel)); }

/*
 * 386 processor status longword.
 */
#define PSL_C 0x00000001    /* carry flag */
#define PSL_PF 0x00000004   /* parity flag */
#define PSL_AF 0x00000010   /* auxiliary carry flag */
#define PSL_Z 0x00000040    /* zero flag */
#define PSL_N 0x00000080    /* sign flag */
#define PSL_T 0x00000100    /* trap flag */
#define PSL_I 0x00000200    /* interrupt enable flag */
#define PSL_D 0x00000400    /* direction flag */
#define PSL_V 0x00000800    /* overflow flag */
#define PSL_IOPL 0x00003000 /* i/o privilege level */
#define PSL_NT 0x00004000   /* nested task */
#define PSL_RF 0x00010000   /* resume flag */
#define PSL_VM 0x00020000   /* virtual 8086 mode */
#define PSL_AC 0x00040000   /* alignment check flag */
#define PSL_VIF 0x00080000  /* virtual interrupt enable flag */
#define PSL_VIP 0x00100000  /* virtual interrupt pending flag */
#define PSL_ID 0x00200000   /* identification flag */

extern "C" void syscall_init_asm(void);
extern "C" void syscall_entry(void);
extern "C" void ignore_sysret(void);


#define FS_BASE_MSR 0xC0000100
#define GS_BASE_MSR 0xC0000101
#define KERNEL_GS_BASE 0xC0000102




void cpu::seginit(struct processor_state *c, void *local) {
  if (local == nullptr) local = p2v(phys::alloc());

  // make sure the local information segment is zeroed
  memset(local, 0, PGSIZE);

  auto *gdt = (u64 *)local;
  auto *tss = (u32 *)(((char *)local) + 1024);
  tss[16] = 0x00680000;  // IO Map Base = End of TSS

  tss[0x64] |= (0x64 * sizeof(u32)) << 16;


  wrmsr(GS_BASE_MSR, ((u64)local) + (PGSIZE / 2));

  // zero out the CPU
  // struct processor_state *c = &cpus[processor_count++];

  __set_cpu_struct(c);

  c->local = local;
  c->kstat.ticks = 0;
  c->active = 1;

	cpu::add(c);

  auto addr = (u64)tss;
  gdt[0] = 0x0000000000000000;
  gdt[SEG_KCODE] = 0x002098000000FFFF;  // Code, DPL=0, R/X
  gdt[SEG_UCODE] = 0x0020F8000000FFFF;  // Code, DPL=3, R/X
  gdt[SEG_KDATA] = 0x000092000000FFFF;  // Data, DPL=0, W
  gdt[SEG_KCPU] = 0x000000000000FFFF;   // unused
  gdt[SEG_UDATA] = 0x0000F2000000FFFF;  // Data, DPL=3, W
  gdt[SEG_TSS + 0] = (0x0067) | ((addr & 0xFFFFFF) << 16) | (0x00E9LL << 40) | (((addr >> 24) & 0xFF) << 56);
  gdt[SEG_TSS + 1] = (addr >> 32);

  lgdt((void *)gdt, 8 * sizeof(u64));

  ltr(SEG_TSS << 3);

  // syscall_init_asm();

#define USER_CS ((unsigned long)(SEG_UCODE << 3) + 3)
#define KERN_CS ((unsigned long)(SEG_KCODE << 3) + 0)


  unsigned long star = (USER_CS << 48) | (KERN_CS << 32);
  wrmsr(MSR_STAR, star);
  wrmsr(MSR_LSTAR, (unsigned long)syscall_entry);


  wrmsr(MSR_CSTAR, 0);
  wrmsr(MSR_IA32_SYSENTER_CS, 0);  // invalid seg
  wrmsr(MSR_IA32_SYSENTER_ESP, 0ULL);
  wrmsr(MSR_IA32_SYSENTER_EIP, 0ULL);

  // FMASK
  wrmsr(0xC0000084, 0x200);
}

static void tss_set_rsp(u32 *tss, u32 n, u64 rsp) {
  tss[n * 2 + 1] = rsp;
  tss[n * 2 + 2] = rsp >> 32;
}

void cpu::switch_vm(ck::ref<struct Thread> thd) {
  auto c = current();
  auto tss = (u32 *)(((char *)c.local) + 1024);

  auto &stk = thd->stacks.last();
  auto stack_addr = (u64)stk.start + stk.size + 8;
  *(off_t *)c.local = stack_addr;



  // When in the kernel, gs is set to one designated by the thread.
  // This is swapped out for a user one (in KERNEL_GS_BASE), upon
  // entering userspace, and swapped back when you enter the kernel
  // on a trap or systemcall.

  // TODO: a real gs thing :)
  uint64_t kernel_gs = 0xFFFFFFFF'00000000 | thd->tid;
  uint64_t user_gs = 0xBBBBBBBB'00000000 | thd->tid;

  switch (thd->proc.ring) {
    case RING_KERN:
      tss_set_rsp(tss, 0, 0);
      break;

    case RING_USER:
      tss_set_rsp(tss, 0, stack_addr);
      break;

    default:
      panic("unknown ring %d in cpu::switch_vm\n", thd->proc.ring);
      break;
  }

  thd->proc.mm->switch_to();

  // load the TLS :)
  if (thd->tls_uaddr != 0) wrmsr(FS_BASE_MSR, thd->tls_uaddr);
}

struct processor_state &cpu::current() {
  return *__get_cpu_struct();
}

