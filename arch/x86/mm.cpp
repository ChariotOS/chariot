#include <errno.h>
#include <mem.h>
#include <mm.h>
#include <multiboot2.h>
#include <phys.h>
#include <types.h>
#include <util.h>
#include <module.h>
#include <x86/mm.h>

#define round_down(x, y) ((x) & ~((y)-1))


static inline void flush_tlb_single(u64 addr) { __asm__ volatile("invlpg (%0)" ::"r"(addr) : "memory"); }


extern u8 *kheap_start;
extern u64 kheap_size;



extern char low_kern_start;
extern char low_kern_end;
extern char high_kern_start;
extern char high_kern_end[];
extern "C" uint64_t boot_p4[];

// just 64 memory regions. Any more and idk what to do
#define MAX_MMAP_ENTRIES 64
static int memory_map_count = 0;
static struct mem_map_entry memory_map[MAX_MMAP_ENTRIES] = {{0}};
static struct mmap_info mm_info;



u64 *kernel_page_table = boot_p4;

int kmem_revision = 0;



static ck::atom<uint64_t> next_ptid = 0;
x86::PageTable::PageTable(u64 *pml4) : pml4(pml4) {
  auto kptable = (u64 *)p2v(kernel_page_table);
  auto pptable = (u64 *)p2v(pml4);

  this->ptid = next_ptid++;

  // TODO: do this with pagefaults instead
  // TODO: maybe flush the tlb if it changes?
  if (kptable != pptable) {
    for (int i = 272; i < 512; i++) {
      if (kptable[i] == 0) break;  // optimization
      pptable[i] = kptable[i];
    }
  }
}
x86::PageTable::~PageTable(void) { x86::free_table(pml4); }

bool x86::PageTable::switch_to(void) {
  // scoped_irqlock l(lock);
  auto kptable = (u64 *)p2v(kernel_page_table);
  auto pptable = (u64 *)p2v(pml4);


  write_cr3((u64)v2p(pml4));
  return true;
}

ck::ref<mm::PageTable> mm::PageTable::create() {
  u64 *pml4 = (u64 *)p2v(phys::alloc(1));
  return ck::make_ref<x86::PageTable>(pml4);
}



int x86::PageTable::get_mapping(off_t va, struct mm::pte &r) {
  scoped_irqlock l(lock);
  off_t pte = *x86::find_mapping(pml4, va, x86::pgsize::page);

  r.prot = PROT_READ;
  if (pte & PTE_W) r.prot |= PROT_WRITE;
  if ((pte & PTE_NX) == 0) r.prot |= PROT_EXEC;

  r.ppn = pte >> 12;

  return 0;
}


int x86::PageTable::del_mapping(off_t va) {
  // scoped_irqlock l(lock);
	assert(in_transaction);
  *x86::find_mapping(pml4, va, x86::pgsize::page) = 0;
  flush_tlb_single(va);
  return 0;
}


int x86::PageTable::add_mapping(off_t va, struct mm::pte &p) {
	assert(in_transaction);
  pending_mappings.push({.va = va, .pte = p});
  return 0;
}

void x86::PageTable::transaction_begin(const char *reason) {
  lock.lock();
	tx_reason = reason;
	in_transaction = true;
  // printf(">>> %d %s tx begin\n", ptid, tx_reason);
}

void x86::PageTable::transaction_commit() {
  for (auto &pm : pending_mappings) {
		auto va = pm.va;
		auto &p = pm.pte;
    uint64_t flags = PTE_P;
    // TOOD: if (p.prot | PROT_READ)
    if (va < CONFIG_KERNEL_VIRTUAL_BASE) flags |= PTE_U;
    if (p.prot & PROT_WRITE) flags |= PTE_W;
    if ((p.prot & PROT_EXEC) == 0) {
      flags |= PTE_NX;
    }

    if (p.nocache) {
      flags |= PTE_D;
    }
    if (p.writethrough) {
      flags |= PTE_PWT;
    }

		// printf("... %d %s map %p\n", ptid, tx_reason, va);
    map_into(pml4, va, p.ppn << 12, x86::pgsize::page, flags);
  }
  pending_mappings.clear();

	in_transaction = false;
  // printf("<<< %d %s tx commit\n\n", ptid, tx_reason);
	tx_reason = NULL;
  lock.unlock();
}



static const char *mem_region_types(int type) {
  switch (type) {
    case MULTIBOOT_MEMORY_AVAILABLE:
      return "usable RAM";
    case MULTIBOOT_MEMORY_RESERVED:
      return "reserved";
    case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
      return "ACPI reclaimable";
    case MULTIBOOT_MEMORY_NVS:
      return "non-volatile storage";
    case MULTIBOOT_MEMORY_BADRAM:
      return "bad RAM";
    default:
      return "UNKNOWN";
  }
}



static uint8_t *kend = (uint8_t *)high_kern_end;
inline void *early_bump_allocate(size_t size) {
  auto p = (void *)kend;
  kend += size;
  return p;
}



void arch_mem_init(unsigned long mbd) {
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


  for (mmap = ((struct multiboot_tag_mmap *)tag)->entries; (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
       mmap = (multiboot_memory_map_t *)((unsigned long)mmap + ((struct multiboot_tag_mmap *)tag)->entry_size)) {
    u64 start, end;

    start = round_up(mmap->addr, 4096);
    end = round_down(mmap->addr + mmap->len, 4096);

    memory_map[n].addr = start;
    memory_map[n].len = end - start;
    memory_map[n].type = mmap->type;

    KINFO("mem: [0x%p-0x%p] %s\n", start, end, mem_region_types(mmap->type));

    total_mem += end - start;

    if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

    if (n > MAX_MMAP_ENTRIES) {
      printf("Reached memory region limit!\n");
      while (1)
        ;
    }

    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) mm_info.usable_ram += mmap->len;

    if (end > (mm_info.last_pfn << 12)) mm_info.last_pfn = end >> 12;

    mm_info.total_mem += end - start;
    ++mm_info.num_regions;
    ++n;
  }
  memory_map_count = n;



  /* Map whichever is larger, the physical memory or 32gb */
  off_t mem_end = max(mm_info.last_pfn << 12, 32L * 1024 * 1024 * 1024);
  /* We mapped 1GiB in arch/x86/asm/boot.asm, so we don't need to do that again */
  size_t nmapped = 4096L * 512L * 512L;
  if (mem_end > nmapped) {
    /* Right now, we have 2mib identity mapped, so quickly go through and map the entirety of memory
     */
    auto *p4 = ((u64 *)read_cr3());
    uint64_t *p3 = (uint64_t *)(p4[0] & ~0xFFF);


    /* Fill out p3 with newly allocated p2's to map all of memory virtually */
    for (int i = 1; nmapped < mem_end; i++) {
      uint64_t *p2 = (uint64_t *)early_bump_allocate(4096);
      for (int o = 0; o < 512; o++) {
        /* Next page to map */
        uint64_t next_pa = nmapped;
        p2[o] = next_pa | PTE_P | PTE_W | PTE_PS;
        nmapped += 4096 * 512;
      }

      p3[i] = (uint64_t)p2 | PTE_P | PTE_W;
    }
    arch_flush_mmu();  // flush out the TLB
  }


  // setup memory regions
  for (int i = 0; i < mm_info.num_regions; i++) {
    auto &region = memory_map[i];
    auto *start = (uint8_t *)region.addr;
    auto *end = start + region.len;

    if (end < kend) continue;
    if (start < kend) start = kend + PGSIZE;

    phys::free_range(start, end);
  }

  printf(KERN_INFO "Detected and mapped physical memory virtually.\n");
}


ksh_def("mmap", "display the physical memory map of the system (only includes usable ram)") {
  for (int i = 0; i < memory_map_count; i++) {
    KINFO("mem: [0x%p-0x%p] %s\n", memory_map[i].addr, memory_map[i].addr + memory_map[i].len, mem_region_types(memory_map[i].type));
  }

  return 0;
}

mm::AddressSpace *kspace;

mm::AddressSpace &mm::AddressSpace::kernel_space(void) {
  if (kspace == NULL) {
    auto kptable = ck::make_ref<x86::PageTable>(kernel_page_table);
    kspace = new mm::AddressSpace(CONFIG_KERNEL_VIRTUAL_BASE, CONFIG_KERNEL_VIRTUAL_BASE + 0x100000000, kptable);
    kspace->is_kspace = 1;
  }

  return *kspace;
}
