#include <lock.h>
#include <mem.h>
#include <multiboot.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

// #define MEM_DEBUG
// #define MEM_TRACE

#ifdef MEM_DEBUG
#define INFO(fmt, args...) printk("[MEM] " fmt, ##args)
#else
#define INFO(fmt, args...)
#endif

#ifdef MEM_TRACE
#define TRACE INFO("TRACE: (%d) %s\n", __LINE__, __PRETTY_FUNCTION__)
#else
#define TRACE
#endif

#define MAX_MMAP_ENTRIES 64

/*
 * Round x up to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))

/*
 * Round x down to the nearest y aligned boundary.  y must be a power of two.
 */
#define round_down(x, y) ((x) & ~((y)-1))

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

bool use_kernel_vm = false;

extern char low_kern_start;
extern char low_kern_end;
extern char high_kern_start;
extern char high_kern_end;

// just 64 memory regions. Any more and idk what to do
static struct mem_map_entry memory_map[MAX_MMAP_ENTRIES];
static struct mmap_info mm_info;

multiboot_info_t *multiboot_info_ptr;

void init_mmap(u64 mbd) {
  multiboot_info_ptr = (multiboot_info_t *)mbd;

  size_t total_mem = 0;
  uint32_t n = 0;

  if (mbd & 7) {
    panic("ERROR: Unaligned multiboot info struct\n");
  }

  const char *names[] = {
      "UNKNOWN",
      [MULTIBOOT_MEMORY_AVAILABLE] = "AVAILABLE",
      [MULTIBOOT_MEMORY_RESERVED] = "RESERVED ",
      [MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = "ACPI RECL",
      [MULTIBOOT_MEMORY_NVS] = "NVS      ",
      [MULTIBOOT_MEMORY_BADRAM] = "BAD RAM  ",
  };

  printk("  - MEMORY:\n");

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

    printk("  %s range=[% 16lx:% 16lx] size=%zu bytes\n", names[mmap->type],
           start, end, end - start);

    total_mem += end - start;

    if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;

    if (n > MAX_MMAP_ENTRIES) {
      printk("Reached memory region limit!\n");
      while (1)
        ;
    }

    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
      mm_info.usable_ram += mmap->len;
    }

    if (end > (mm_info.last_pfn << 12)) {
      mm_info.last_pfn = end >> 12;
    }

    mm_info.total_mem += end - start;
    ++mm_info.num_regions;
    ++n;
  }

  printk("\n");
  printk("  - total:    %zu\n", total_mem);
  printk("  - usable:   %zu\n", mm_info.total_mem);
  printk("  - reserved: %zu\n", total_mem - mm_info.total_mem);
  printk("\n");
}

size_t mem_size() { return mm_info.total_mem; }

int init_mem(u64 mbd) {
  // go detect all the ram in the system
  init_mmap(mbd);

#ifdef MEM_DEBUG
  char buf[20];
  INFO("Detected %s of usable memory (%lu pages)\n",
       human_size(mm_info.usable_ram, buf), mm_info.usable_ram / 4096);
#endif

  auto *kend = (u8 *)&high_kern_end;

  // setup memory regions
  for (int i = 0; i < mm_info.num_regions; i++) {
    auto &region = memory_map[i];
    auto *start = (u8 *)region.addr;
    auto *end = start + region.len;

    if (end < kend) continue;
    if (start < kend) start = kend + PGSIZE;

    phys::free_range(start, end);
  }

  return 0;
}

void *alloc_id_page() {
  auto pa = phys::alloc();
  return pa;
}

static u8 *kheap_start = NULL;
static u64 kheap_size = 0;

void *kheap_lo(void) { return kheap_start; }
void *kheap_hi(void) { return kheap_start + kheap_size; }

#define PGROUNDUP(x) round_up(x, 4096)

/**
 * ksbrk - shift the end of the heap further.
 */
void *ksbrk(i64 inc) {
  // printk("sbrk %d: %p (%zu)\n", inc, kheap_start + kheap_size, kheap_size);
  u64 oldsz = kheap_size;
  u64 newsz = oldsz + inc;

  if (inc == 0) return kheap_start + oldsz;

  i64 a = PGROUNDUP(oldsz);
  for (; a < newsz; a += 4096) {
    void *pa = phys::alloc();
    paging::map((u64)kheap_start + a, (u64)pa, paging::pgsize::page,
                PTE_W | PTE_P);
  }
  kheap_size = newsz;
  return kheap_start + oldsz;
}

extern int mm_init(void);

void init_kernel_virtual_memory() {
#ifdef MEM_DEBUF
  char buf[50];
#endif

  INFO("has: %s\n", human_size(mm_info.total_mem, buf));

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

  for (; true; i += page_step) {
    if (i > max(mm_info.total_mem, min_mem)) break;
    paging::map_into(new_cr3, (u64)p2v(i), i, page_size, PTE_W | PTE_P);
    // printk("%p\n", i);
  }

  INFO("mapped %s into high kernel memory\n", human_size(i, buf));

  kheap_start = (u8 *)p2v(i);
  kheap_size = 0;

  // copy the low mapping
  new_cr3[0] = ((u64 *)read_cr3())[0];

  // enable kernel vm, so paging can stop mapping to the scratch page, and we
  // can also reference all of physical memory with virtual memory
  use_kernel_vm = true;
  write_cr3((u64)new_cr3);
  tlb_flush();  // flush out the TLB
  // initialize the memory allocator
  mm_init();
}

extern "C" void *malloc(size_t size);
extern "C" void free(void *ptr);
extern "C" void *realloc(void *ptr, size_t size);

extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

static mutex_lock s_allocator_lock;

void *kmalloc(u64 size) {
  s_allocator_lock.lock();
  auto ptr = mm_malloc(size);
  s_allocator_lock.unlock();
  return ptr;
}
void kfree(void *ptr) {
  s_allocator_lock.lock();
  auto p = (u64)ptr;
  if (ptr != NULL) {
    if (!(p >= (u64)kheap_lo() && p < (u64)kheap_hi())) {
      printk("invalid address passed into free: %p\n", ptr);
    }
    mm_free(ptr);
  }
  s_allocator_lock.unlock();
}

void *krealloc(void *ptr, u64 newsize) {
  s_allocator_lock.lock();
  auto p = mm_realloc(ptr, newsize);
  s_allocator_lock.unlock();
  return p;
}
