#include <mm.h>


mm::area::area(void) {}


mm::area::~area(void) {
  off_t n = 0;
  for (auto &p : pages) {
    if (p) {
      spinlock::lock(p->lock);
      p->users--;
      // if the region was dirty, and we have an object, notify them and ask
      // them to flush the nth page
      if (p->dirty && obj) {
        obj->flush(n);
      }
      spinlock::unlock(p->lock);
    }
    n += 1;
  }


  pages.clear();
  // release the object if we have one
  if (obj) {
    obj->release();
  }
  obj = nullptr;
}
