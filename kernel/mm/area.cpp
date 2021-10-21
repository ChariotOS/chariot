#include <mm.h>


mm::MappedRegion::MappedRegion(void) {}


mm::MappedRegion::~MappedRegion(void) {
  for (int i = 0; i < mappings.size(); i++) {
    auto &m = mappings[i];
    if (m) {
      m->lock();
      // if the region was dirty, and we have an object, notify them and ask
      // them to flush the nth page
      if (m->fcheck(PG_DIRTY) && obj) {
        obj->flush(i);
      }
      m->unlock();
    }
    mappings[i].set_page(nullptr);
  }

  mappings.clear();

  // release the object if we have one
  if (obj) {
    obj->release();
    obj = nullptr;
  }
}
