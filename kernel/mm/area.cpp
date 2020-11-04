#include <mm.h>


mm::area::area(void) {}


mm::area::~area(void) {
  for (int i = 0; i < pages.size(); i++) {
    auto &p = pages[i];
    if (p) {
      spinlock::lock(p->lock);
      p->users--;
      // if the region was dirty, and we have an object, notify them and ask
      // them to flush the nth page
      if (p->dirty && obj) {
        obj->flush(i);
      }
      spinlock::unlock(p->lock);
    }
    pages[i] = nullptr;
  }

	pages.clear();

  // release the object if we have one
  if (obj) {
    obj->release();
  }
  obj = nullptr;
}
