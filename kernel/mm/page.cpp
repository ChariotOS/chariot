#include <cpu.h>
#include <mm.h>
#include <phys.h>




static spinlock pg_tree_lock;
static rb_root pg_tree = RB_ROOT;


static void remove_page_from_tree(mm::page &pg) {

#ifdef PAGE_ENABLE_RBTREE
  scoped_irqlock l(pg_tree_lock);
  rb_erase(&pg.rb_node, &pg_tree);
#endif

#if 0

  int count = 0;
  for (struct rb_node *node = rb_first(&pg_tree); node; node = rb_next(node)) {
    auto *self = rb_entry(node, struct mm::page, rb_node);
    count++;
    printk("%4d pa:%p u:%2d", count, self->pa, self->users);
    if (self->fcheck(PG_BCACHE))
      printk(" BCACHE");
    else
      printk(" ANON");

    if (self->fcheck(PG_DIRTY)) printk(" DIRTY");
    printk("\n");
  }
  printk("\n");
#endif
}

static void update_page_address(mm::page *pg, unsigned long pa) {
  pg->pa = pa;


#ifdef PAGE_ENABLE_RBTREE
  scoped_irqlock l(pg_tree_lock);

  // printk("update page address %p\n", pa);

  pg->pa = pa;
  struct rb_node **n = &(pg_tree.rb_node);
  struct rb_node *parent = NULL;


  int steps = 0;
  /* Figure out where to put new node */
  while (*n != NULL) {
    auto *self = rb_entry(*n, struct mm::page, rb_node);
    long result = (long)pg->pa - (long)self->pa;

    parent = *n;

    steps++;
    if (result < 0)
      n = &((*n)->rb_left);
    else if (result > 0)
      n = &((*n)->rb_right);
    else {
      panic("PA %p is already in the pg_tree\n", pa);
    }
  }

  /* Add new node and rebalance tree. */
  rb_link_node(&pg->rb_node, parent, n);
  rb_insert_color(&pg->rb_node, &pg_tree);
  // printk("%p steps:%d\n", pa, steps);
#endif
}

mm::page::page(void) {
  fclr(PG_WRTHRU | PG_NOCACHE | PG_DIRTY);
  pa = 0;
	/*
  lru = cpu::get_ticks();
	auto bf = phys::bytes_free();
	printk_nolock("nfree: %12zu bytes: %p\n", bf, bf);
	*/
}

mm::page::~page(void) {
  if (fcheck(PG_OWNED) && (void *)pa != NULL) {
    phys::free((void *)pa);
  }
  remove_page_from_tree(*this);
  pa = 0;
}

ref<mm::page> mm::page::alloc(void) {
  auto p = make_ref<mm::page>();
  update_page_address(p.get(), (unsigned long)phys::alloc());
  p->users = 0;

  // setup default flags
  p->fset(PG_OWNED);
  return move(p);
}

ref<mm::page> mm::page::create(unsigned long page) {
  auto p = make_ref<mm::page>();

  update_page_address(p.get(), page);
  p->users = 0;
  // setup default flags
  p->fclr(PG_OWNED);
  return move(p);
}
