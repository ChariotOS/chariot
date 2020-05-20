#include <errno.h>
#include <mem.h>
#include <mm.h>
#include <multiboot.h>
#include <paging.h>
#include <phys.h>
#include <util.h>
#include <types.h>

#define round_down(x, y) ((x) & ~((y)-1))
extern int mm_init(void);

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
  pagetable(u64 *pml4) : pml4(pml4) {}
  virtual ~pagetable();

  virtual bool switch_to(void) override;

  virtual int add_mapping(off_t va, struct mm::pte &) override;
  virtual int get_mapping(off_t va, struct mm::pte &) override;
  virtual int del_mapping(off_t va) override;
};
}  // namespace x86

x86::pagetable::~pagetable(void) {
  paging::free_table(pml4);
}

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
  return 0;
}

/**
 * ksbrk - shift the end of the heap further.
 */
void *ksbrk(i64 inc) {
  // printk("sbrk %d: %p (%zu)\n", inc, kheap_start + kheap_size, kheap_size);
  u64 oldsz = kheap_size;
  u64 newsz = oldsz + inc;

  if (inc == 0) return kheap_start + oldsz;

  i64 a = round_up(oldsz, 4096);
  for (; a < newsz; a += 4096) {
    void *pa = phys::alloc();
    paging::map_into((u64 *)kernel_page_table, (u64)kheap_start + a, (u64)pa,
                     paging::pgsize::page, PTE_W | PTE_P);
  }
  kheap_size = newsz;

  return kheap_start + oldsz;
}


void arch::mem_init(unsigned long mbd) {
  multiboot_info_ptr = (multiboot_info_t *)mbd;

  size_t total_mem = 0;
  uint32_t n = 0;

  if (mbd & 7) {
    panic("ERROR: Unaligned multiboot info struct\n");
  }

  const char *names[] = {
      "UNKNOWN",
      [MULTIBOOT_MEMORY_AVAILABLE] = "usable",
      [MULTIBOOT_MEMORY_RESERVED] = "reserved",
      [MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = "reserved (acpi recl)",
      [MULTIBOOT_MEMORY_NVS] = "reserved (nvs)",
      [MULTIBOOT_MEMORY_BADRAM] = "bad ram",
  };
  KINFO("Physical Memory Map:\n");

  for (auto *mmap =
           (multiboot_memory_map_t *)(u64)multiboot_info_ptr->mmap_addr;
       (unsigned long)mmap <
       multiboot_info_ptr->mmap_addr + multiboot_info_ptr->mmap_length;
       mmap = (multiboot_memory_map_t *)((unsigned long)mmap + mmap->size +
                                         sizeof(mmap->size))) {
    u64 start, end;

    start = round_up(mmap->addr, 4096);
    end = round_down(mmap->addr + mmap->len, 4096);

    memory_map[n].addr = start;
    memory_map[n].len = end - start;
    memory_map[n].type = mmap->type;

    KINFO("mem: [0x%p-0x%p] %s\n", start, end, names[mmap->type]);

    total_mem += end - start;

    if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

    if (n > MAX_MMAP_ENTRIES) {
      printk("Reached memory region limit!\n");
      while (1)
        ;
    }

    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE)
      mm_info.usable_ram += mmap->len;

    if (end > (mm_info.last_pfn << 12)) mm_info.last_pfn = end >> 12;

    mm_info.total_mem += end - start;
    ++mm_info.num_regions;
    ++n;
  }

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

  kheap_start = (u8 *)p2v(i);
  kheap_size = 0;

  // copy the low mapping
  new_cr3[0] = ((u64 *)read_cr3())[0];

  // enable kernel vm, so paging can stop mapping to the scratch page, and we
  // can also reference all of physical memory with virtual memory
  use_kernel_vm = true;
  write_cr3((u64)new_cr3);
  arch::flush_tlb();  // flush out the TLB
  // initialize the memory allocator
  mm_init();
}

mm::space *kspace;

mm::space &mm::space::kernel_space(void) {
  if (kspace == NULL) {
    auto kptable = make_ref<x86::pagetable>(kernel_page_table);
    kspace = new mm::space(KERNEL_VIRTUAL_BASE,
                           KERNEL_VIRTUAL_BASE + 0x100000000, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
