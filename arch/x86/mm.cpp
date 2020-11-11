#include <errno.h>
#include <mem.h>
#include <mm.h>
#include <multiboot2.h>
#include <paging.h>
#include <phys.h>
#include <types.h>
#include <util.h>

#define round_down(x, y) ((x) & ~((y)-1))


static inline void flush_tlb_single(u64 addr) { __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory"); }


extern u8 *kheap_start;
extern u64 kheap_size;

#define MAX_MMAP_ENTRIES 64

bool use_kernel_vm = false;

extern char low_kern_start;
extern char low_kern_end;
extern char high_kern_start;
extern char high_kern_end[];

// just 64 memory regions. Any more and idk what to do
static struct mem_map_entry memory_map[MAX_MMAP_ENTRIES];
static struct mmap_info mm_info;

u64 *kernel_page_table;

int kmem_revision = 0;

namespace x86 {
  class pagetable : public mm::pagetable {
    u64 *pml4;

   public:
    pagetable(u64 *pml4);
    virtual ~pagetable();

    virtual bool switch_to(void) override;

    virtual int add_mapping(off_t va, struct mm::pte &) override;
    virtual int get_mapping(off_t va, struct mm::pte &) override;
    virtual int del_mapping(off_t va) override;
  };
}  // namespace x86


x86::pagetable::pagetable(u64 *pml4) : pml4(pml4) {}
x86::pagetable::~pagetable(void) { paging::free_table(pml4); }

bool x86::pagetable::switch_to(void) {
  auto kptable = (u64 *)p2v(kernel_page_table);
  auto pptable = (u64 *)p2v(pml4);

  // TODO: do this with pagefaults instead
  // TODO: maybe flush the tlb if it changes?
  if (kptable != pptable) {
    for (int i = 272; i < 512; i++) {
      if (kptable[i] == 0) break;  // optimization
      pptable[i] = kptable[i];
    }
  }

  write_cr3((u64)v2p(pml4));
  return true;
}

ref<mm::pagetable> mm::pagetable::create() {
  u64 *pml4 = (u64 *)p2v(phys::alloc(1));
  return make_ref<x86::pagetable>(pml4);
}

int x86::pagetable::add_mapping(off_t va, struct mm::pte &p) {
  int flags = PTE_P;
  // TOOD: if (p.prot | PROT_READ)
  if (va < KERNEL_VIRTUAL_BASE) flags |= PTE_U;
  if (p.prot & PROT_WRITE) flags |= PTE_W;
  if ((p.prot & PROT_EXEC) == 0) flags |= PTE_NX;

  if (p.nocache) {
    // printk("nocache\n");
    flags |= PTE_D;
  }
  if (p.writethrough) {
    // printk("writethrough\n");
    flags |= PTE_PWT;
  }

  map_into(pml4, va, p.ppn << 12, paging::pgsize::page, flags);

  return 0;
}
int x86::pagetable::get_mapping(off_t va, struct mm::pte &r) {
  off_t pte = *paging::find_mapping(pml4, va, paging::pgsize::page);

  r.prot = PROT_READ;
  if (pte & PTE_W) r.prot |= PROT_WRITE;
  if ((pte & PTE_NX) == 0) r.prot |= PROT_EXEC;

  r.ppn = pte >> 12;

  return 0;
}
int x86::pagetable::del_mapping(off_t va) {
  *paging::find_mapping(pml4, va, paging::pgsize::page) = 0;

  flush_tlb_single(va);
  return 0;
}

const char *mem_region_types[] = {
    [0] = "unknown",
    [MULTIBOOT_MEMORY_AVAILABLE] = "usable RAM",
    [MULTIBOOT_MEMORY_RESERVED] = "reserved",
    [MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = "ACPI reclaimable",
    [MULTIBOOT_MEMORY_NVS] = "non-volatile storage",
    [MULTIBOOT_MEMORY_BADRAM] = "bad RAM",
};


void arch::mem_init(unsigned long mbd) {
#ifdef CHARIOT_HRT
  phys::free_range((void *)0x184000, (void *)0x1ffe0000);
  mm_info.total_mem = 0x1ffe0000;
#else


  struct multiboot_tag *tag;
  size_t total_mem = 0;
  uint32_t n = 0;

  if (mbd & 7) {
    panic("ERROR: Unaligned multiboot info struct\n");
  }

  tag = (struct multiboot_tag *)(mbd + 8);
  while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag + ((tag->size + 7) & ~7));
  }

  if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    panic("ERROR: no mmap tag found\n");
  }



  multiboot_memory_map_t *mmap;

  for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
       (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
       mmap = (multiboot_memory_map_t *)((unsigned long)mmap + ((struct multiboot_tag_mmap *)tag)->entry_size)) {
    u64 start, end;

    start = round_up(mmap->addr, 4096);
    end = round_down(mmap->addr + mmap->len, 4096);

    memory_map[n].addr = start;
    memory_map[n].len = end - start;
    memory_map[n].type = mmap->type;

    KINFO("mem: [0x%p-0x%p] %s\n", start, end, mem_region_types[mmap->type]);

    total_mem += end - start;

    if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

    if (n > MAX_MMAP_ENTRIES) {
      printk("Reached memory region limit!\n");
      while (1)
        ;
    }

    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) mm_info.usable_ram += mmap->len;

    if (end > (mm_info.last_pfn << 12)) mm_info.last_pfn = end >> 12;

    mm_info.total_mem += end - start;
    ++mm_info.num_regions;
    ++n;
  }
#endif

  auto *kend = (u8 *)high_kern_end;

  // setup memory regions
  for (int i = 0; i < mm_info.num_regions; i++) {
    auto &region = memory_map[i];
    auto *start = (u8 *)region.addr;
    auto *end = start + region.len;

    if (end < kend) continue;
    if (start < kend) start = kend + PGSIZE;

    phys::free_range(start, end);
  }

  u64 page_step = PAGE_SIZE;
  auto page_size = paging::pgsize::page;

  bool use_large = true;
  bool use_huge = false;

  if (use_large) {
    page_step = LARGE_PAGE_SIZE;
    page_size = paging::pgsize::large;
  } else if (use_huge) {
    page_step = HUGE_PAGE_SIZE;
    page_size = paging::pgsize::huge;
  }

  u64 i = 0;

  // so the min_mem is the miniumum amount of memory to map
  size_t min_mem = 4l * 1024l * 1024l * 1024l;

  auto *new_cr3 = (u64 *)phys::alloc();

  kernel_page_table = (u64 *)p2v(new_cr3);

  off_t lo = 0;
  off_t hi = 0;
  while (1) {
    if (i > max(mm_info.total_mem, min_mem)) break;
    hi = i;
    // printk("%p <- %p\n", i, p2v(i));
    paging::map_into(new_cr3, (u64)p2v(i), i, page_size, PTE_W | PTE_P);
    i += page_step;
  }

  mm_info.vmlo = (off_t)p2v(lo);
  mm_info.vmhi = (off_t)p2v(hi);

  // copy the low mapping
  new_cr3[0] = ((u64 *)read_cr3())[0];

  // enable kernel vm, so paging can stop mapping to the scratch page, and we
  // can also reference all of physical memory with virtual memory
  use_kernel_vm = true;
  write_cr3((u64)new_cr3);
  arch::flush_tlb();  // flush out the TLB
}

mm::space *kspace;

mm::space &mm::space::kernel_space(void) {
  if (kspace == NULL) {
    auto kptable = make_ref<x86::pagetable>(kernel_page_table);
    kspace = new mm::space(KERNEL_VIRTUAL_BASE, KERNEL_VIRTUAL_BASE + 0x100000000, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
