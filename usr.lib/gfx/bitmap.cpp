#include <chariot/mshare.h>
#include <gfx/bitmap.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

void *mshare_create(const char *name, size_t size) {
  struct mshare_create arg;
  arg.size = size;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);
  return (void *)syscall(SYS_mshare, MSHARE_CREATE, &arg);
}


void *mshare_acquire(const char *name, size_t *size) {
  struct mshare_acquire arg;
  arg.size = 0;
  strncpy(arg.name, name, MSHARE_NAMESZ - 1);

  if (size != NULL) *size = arg.size;
  return (void *)syscall(SYS_mshare, MSHARE_ACQUIRE, &arg);
}


unsigned long read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}

static ck::string unique_ident(void) {
  static long next_id = 0;
  char buf[50];
  snprintf(buf, 50, "%d:%d-%llx", getpid(), next_id++, read_timestamp());

  return buf;
}



gfx::bitmap::bitmap(size_t w, size_t h) : m_width(w), m_height(h) {
  m_pixels = (uint32_t *)mmap(NULL, size(), PROT_READ | PROT_WRITE,
                              MAP_ANON | MAP_PRIVATE, -1, 0);
}


gfx::bitmap::~bitmap(void) {
  if (m_pixels) {
    munmap(m_pixels, size());
  }
}


gfx::shared_bitmap::shared_bitmap(size_t w, size_t h) : m_name(unique_ident()) {
  m_height = h;
  m_width = w;
  m_pixels = (uint32_t *)mshare_create(shared_name(), size());
}


gfx::shared_bitmap::~shared_bitmap(void) {
  if (m_pixels) {
    munmap(m_pixels, size());
  }
}
