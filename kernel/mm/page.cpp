#include <cpu.h>
#include <mm.h>
#include <phys.h>

mm::page::page(void) {
  writethrough = false;
  nocache = false;
  lru = cpu::get_ticks();
}

mm::page::~page(void) {
  if (freeable && (void *)pa != NULL) {
    phys::free((void *)pa);
  }
  pa = 0;
}

ref<mm::page> mm::page::alloc(void) {
  auto p = make_ref<mm::page>();
  p->pa = (u64)phys::alloc();
  p->users = 0;

  // setup default flags
  p->freeable = 1;
  return move(p);
}

ref<mm::page> mm::page::create(unsigned long page) {
  auto p = make_ref<mm::page>();
  p->pa = page;
  p->users = 0;
  // setup default flags
  p->freeable = 0;
  return move(p);
}
