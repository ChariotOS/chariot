#include <cpu.h>
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

int kmem_revision = 0;

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

    KINFO("%zu bytes %s - %lx:%lx\n", end - start, names[mmap->type], start,
          end);

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

  /*
  KINFO("\n");
  KINFO("  - total:    %zu\n", total_mem);
  KINFO("  - usable:   %zu\n", mm_info.total_mem);
  KINFO("  - reserved: %zu\n", total_mem - mm_info.total_mem);
  KINFO("\n");
  */
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

static void *kernel_page_table;

void init_kernel_virtual_memory() {
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

  kernel_page_table = p2v(new_cr3);

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
  tlb_flush();  // flush out the TLB
  // initialize the memory allocator
  mm_init();
}

void *get_kernel_page_table(void) { return kernel_page_table; }

extern "C" void *malloc(size_t size);
extern "C" void free(void *ptr);
extern "C" void *realloc(void *ptr, size_t size);

extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);

static spinlock s_allocator_lock("allocator");

static void alloc_lock(void) { s_allocator_lock.lock(); }

static void alloc_unlock(void) { s_allocator_lock.unlock(); }

void *kmalloc(u64 size) {
  alloc_lock();
  auto ptr = mm_malloc(size);
  alloc_unlock();
  return ptr;
}
void kfree(void *ptr) {
  alloc_lock();
  auto p = (u64)ptr;
  if (ptr != NULL) {
    if (!(p >= (u64)kheap_lo() && p < (u64)kheap_hi())) {
      printk("invalid address passed into free: %p\n", ptr);
    }
    mm_free(ptr);
  }
  alloc_unlock();
}

void *krealloc(void *ptr, u64 newsize) {
  alloc_lock();
  auto p = mm_realloc(ptr, newsize);
  alloc_unlock();
  return p;
}

void *memcpy(void *dest, const void *src, size_t n) {
  auto *d = (unsigned char *)dest;
  auto *s = (const unsigned char *)src;

#define LS >>
#define RS <<

  typedef uint32_t __attribute__((__may_alias__)) u32;
  uint32_t w, x;

  for (; (uintptr_t)s % 4 && n; n--) *d++ = *s++;

  if ((uintptr_t)d % 4 == 0) {
    for (; n >= 16; s += 16, d += 16, n -= 16) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      *(u32 *)(d + 8) = *(u32 *)(s + 8);
      *(u32 *)(d + 12) = *(u32 *)(s + 12);
    }
    if (n & 8) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      *(u32 *)(d + 4) = *(u32 *)(s + 4);
      d += 8;
      s += 8;
    }
    if (n & 4) {
      *(u32 *)(d + 0) = *(u32 *)(s + 0);
      d += 4;
      s += 4;
    }
    if (n & 2) {
      *d++ = *s++;
      *d++ = *s++;
    }
    if (n & 1) {
      *d = *s;
    }
    return dest;
  }

  if (n >= 32) switch ((uintptr_t)d % 4) {
      case 1:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        *d++ = *s++;
        n -= 3;
        for (; n >= 17; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 1);
          *(u32 *)(d + 0) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 5);
          *(u32 *)(d + 4) = (x LS 24) | (w RS 8);
          x = *(u32 *)(s + 9);
          *(u32 *)(d + 8) = (w LS 24) | (x RS 8);
          w = *(u32 *)(s + 13);
          *(u32 *)(d + 12) = (x LS 24) | (w RS 8);
        }
        break;
      case 2:
        w = *(u32 *)s;
        *d++ = *s++;
        *d++ = *s++;
        n -= 2;
        for (; n >= 18; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 2);
          *(u32 *)(d + 0) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 6);
          *(u32 *)(d + 4) = (x LS 16) | (w RS 16);
          x = *(u32 *)(s + 10);
          *(u32 *)(d + 8) = (w LS 16) | (x RS 16);
          w = *(u32 *)(s + 14);
          *(u32 *)(d + 12) = (x LS 16) | (w RS 16);
        }
        break;
      case 3:
        w = *(u32 *)s;
        *d++ = *s++;
        n -= 1;
        for (; n >= 19; s += 16, d += 16, n -= 16) {
          x = *(u32 *)(s + 3);
          *(u32 *)(d + 0) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 7);
          *(u32 *)(d + 4) = (x LS 8) | (w RS 24);
          x = *(u32 *)(s + 11);
          *(u32 *)(d + 8) = (w LS 8) | (x RS 24);
          w = *(u32 *)(s + 15);
          *(u32 *)(d + 12) = (x LS 8) | (w RS 24);
        }
        break;
    }
  if (n & 16) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 8) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 4) {
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 2) {
    *d++ = *s++;
    *d++ = *s++;
  }
  if (n & 1) {
    *d = *s;
  }
  return dest;
}
