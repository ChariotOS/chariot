#include <cpu.h>
#include <mm.h>
#include <phys.h>




mm::page::page(void) {
  fclr(PG_WRTHRU | PG_NOCACHE | PG_DIRTY);
  set_pa(0);
}

mm::page::~page(void) {
  if (fcheck(PG_OWNED) && (void *)pa() != NULL) {
    phys::free((void *)pa());
  }
  set_pa(0);
}

ref<mm::page> mm::page::alloc(void) {
  auto p = make_ref<mm::page>();
  p->set_pa((unsigned long)phys::alloc());
  p->m_users = 0;

  // setup default flags
  p->fset(PG_OWNED);
  return move(p);
}

ref<mm::page> mm::page::create(unsigned long page) {
  auto p = make_ref<mm::page>();
  p->set_pa((unsigned long)page);
  p->m_users = 0;
  // setup default flags
  p->fclr(PG_OWNED);
  return move(p);
}
