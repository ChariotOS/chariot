#include <mem.h>
#include <multiboot.h>
#include <paging.h>
#include <phys.h>
#include <printk.h>
#include <types.h>

#define MAX_MMAP_ENTRIES 128

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

// just 128 memory regions. Any more and idk what to do
static struct mem_map_entry memory_map[128];
static struct mmap_info mm_info;

void init_mmap(u64 mbd) {
  struct multiboot_tag *tag;
  uint32_t n = 0;

  if (mbd & 7) {
    printk("ERROR: Unaligned multiboot info struct\n");
  }

  tag = (struct multiboot_tag *)(mbd + 8);
  while (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    tag = (struct multiboot_tag *)((multiboot_uint8_t *)tag +
                                   ((tag->size + 7) & ~7));
  }

  if (tag->type != MULTIBOOT_TAG_TYPE_MMAP) {
    printk("ERROR: no mmap tag found\n");
    while (1)
      ;
  }

  multiboot_memory_map_t *mmap;

  for (mmap = ((struct multiboot_tag_mmap *)tag)->entries;
       (multiboot_uint8_t *)mmap < (multiboot_uint8_t *)tag + tag->size;
       mmap = (multiboot_memory_map_t *)((u64)mmap +
                                         ((struct multiboot_tag_mmap *)tag)
                                             ->entry_size)) {
    if (n > MAX_MMAP_ENTRIES) {
      printk("Reached memory region limit!\n");
      while (1)
        ;
    }

    u64 start, end;

    start = round_up(mmap->addr, 4096);
    end = round_down(mmap->addr + mmap->len, 4096);

    memory_map[n].addr = start;
    memory_map[n].len = end - start;
    memory_map[n].type = mmap->type;

    // printk("mmap[%u] - [%p - %p] <%lu bytes>\n", n, start, end, end - start);

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
}

int init_mem(u64 mbd) {
  // go detect all the ram in the system
  init_mmap(mbd);

  char buf[20];
  printk("Detected %s of usable memory (%lu pages)\n",
         human_size(mm_info.usable_ram, buf), mm_info.usable_ram / 4096);

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

extern "C" void *sbrk(i64 inc) { return ksbrk(inc); }

extern int mm_init(void);

void init_dyn_mm(void) {
  // the kheap starts, virtually, after the last physical page.

  // ksbrk(0);

  mm_init();
}

void init_kernel_virtual_memory() {
  char buf[50];

  // printk("\n\n\n\n\n");
  printk("has: %s\n", human_size(mm_info.total_mem, buf));

  u64 page_step = HUGE_PAGE_SIZE;
  auto page_size = paging::pgsize::huge;

  u64 i = 0;

  for (; true; i += page_step) {
    if (i > mm_info.total_mem + page_step) break;

    // printk("mapping %p to %p\n", (u64)p2v(i), i);
    paging::map((u64)p2v(i), i, page_size, PTE_W | PTE_P);
  }

  printk("mapped %s into high kernel memory\n", human_size(i, buf));

  kheap_start = (u8 *)p2v(i);
  kheap_size = 0;

  ksbrk(0);


  mm_init();

  use_kernel_vm = true;
  tlb_flush();  // flush out the TLB
}

extern "C" void *malloc(size_t size);
extern "C" void free(void *ptr);
extern "C" void *realloc(void *ptr, size_t size);

extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
extern void *mm_realloc(void *ptr, size_t size);




void *kmalloc(u64 size) {
  auto ptr = mm_malloc(size);
  // printk("malloc (%zu) = %p\n", size, ptr);
  return ptr;
}
void kfree(void *ptr) {
  auto p = (u64)ptr;

  if (!(p >= (u64)kheap_lo() && p < (u64)kheap_hi())) {
    printk("invalid address passed into free: %p\n", ptr);
  }
  // printk("free(%p)\n", ptr);
  mm_free(ptr);
}

void *krealloc(void *ptr, u64 newsize) {
  // printk("realloc(%p, %zu)\n", ptr, newsize);
  return mm_realloc(ptr, newsize);
}
