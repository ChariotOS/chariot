#include <gfx/bitmap.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>

unsigned long read_timestamp(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
  uint64_t ret;
  asm volatile("pushfq; popq %0" : "=a"(ret));
  return ret;
}


/*
static ck::string unique_ident(void) {
	char buf[50];

	snprintf(buf, 50, "%d-%llx", getpid(), read_timestamp());

	return buf;
}
*/

ck::ref<gfx::bitmap> gfx::bitmap::create(size_t w, size_t h) {
	return ck::make_ref<gfx::bitmap>(w, h, false);
}


gfx::bitmap::bitmap(size_t w, size_t h, bool shared) : m_width(w), m_height(h), m_shared(shared) {
	// printf("unique ident: %s\n", unique_ident().get());
	if (m_shared) {
		panic("bitmap::create_shared not setup\n");
	} else {
		m_pixels = (uint32_t*)mmap(NULL, size(), PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
	}
}


gfx::bitmap::~bitmap(void) {
	if(m_pixels) {
		munmap(m_pixels, size());
	}
}
