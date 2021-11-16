#include <cpu.h>
#include <mm.h>
#include <phys.h>



mm::Page::Page(void) {
  fclr(PG_WRTHRU | PG_NOCACHE | PG_DIRTY);

  set_pa(0);
}

mm::Page::~Page(void) {
  if (fcheck(PG_OWNED) && (void *)pa() != NULL) {
    phys::free((void *)pa());
  }
  set_pa(0);
}

ck::ref<mm::Page> mm::Page::alloc(void) {
  auto p = ck::make_ref<mm::Page>();
  p->set_pa((unsigned long)phys::alloc());
  p->m_users = 0;

  // setup default flags
  p->fset(PG_OWNED);
  return move(p);
}

ck::ref<mm::Page> mm::Page::create(unsigned long page) {
  auto p = ck::make_ref<mm::Page>();
  p->set_pa((unsigned long)page);
  p->m_users = 0;
  // setup default flags
  p->fclr(PG_OWNED);
  return move(p);
}
