#include <cpu.h>
#include <mm.h>
#include <mshare.h>
#include <phys.h>
#include <syscall.h>
#include <types.h>
#include <util.h>
#include <printf.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))


static spinlock mshare_lock;
static ck::map<ck::string, struct mshare_vmobject *> mshare_regions;


struct mshare_vmobject final : public mm::VMObject {
  mshare_vmobject(ck::string name, size_t npages) : VMObject(npages) {
    this->name = name;
    // printf("CREATE %s!\n", name.get());
    for (int i = 0; i < npages; i++) {
      pages.push(nullptr);
    }
    scoped_lock l(mshare_lock);
    mshare_regions[name] = this;

    // printf("[mshare] new  '%s' %p\n", name.get(), this);
  }

  virtual ~mshare_vmobject(void) {
    // printf("DROP %s! (pid=%d)\n", this->name.get(), curproc->pid);
    scoped_lock l1(mshare_lock);
    scoped_lock l2(m_lock);
    assert(mshare_regions.contains(name));
    mshare_regions.remove(name);
  };


  // get a shared page (page #n in the mapping)
  virtual ck::ref<mm::Page> get_shared(off_t n) override {
    // printf("GET %s! (pid=%d, page=%d)\n", this->name.get(), curproc->pid, n);
    scoped_lock l(m_lock);
    while (n >= pages.size()) {
      pages.push(nullptr);
    }
    if (!pages[n]) pages[n] = mm::Page::alloc();

    return pages[n];
  }


  virtual void drop(void) override {}

 private:
  ck::string name;
  spinlock m_lock;
  ck::vec<ck::ref<mm::Page>> pages;
};



static void *msh_create(struct mshare_create *arg) {
  ck::string name = ck::string(arg->name, MSHARE_NAMESZ);


  if (mshare_regions.contains(name)) return MAP_FAILED;
  if (arg->size & 0xFFF) return MAP_FAILED;

  auto pages = NPAGES(arg->size);

  // this should automatically add it to the global list
  ck::ref<mm::VMObject> obj = ck::make_ref<mshare_vmobject>(name, pages);


  auto addr = curproc->mm->mmap(
      ck::string::format("[msh '%s' (created)]", name.get()), 0, pages * PGSIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, nullptr, 0);

  auto region = curproc->mm->lookup(addr);
  if (region == nullptr) return MAP_FAILED;

  region->obj = obj;
  return (void *)addr;
}




static void *msh_acquire(struct mshare_acquire *arg) {
  ck::string name(arg->name, MSHARE_NAMESZ);


  scoped_lock l(mshare_lock);

  if (!mshare_regions.contains(name)) return MAP_FAILED;
  if (arg->size & 0xFFF) return MAP_FAILED;


  // grab the object
  ck::ref<mm::VMObject> obj = mshare_regions.get(name);

  obj->acquire();

  auto addr = curproc->mm->mmap(
      ck::string::format("[msh '%s' (acquired)]", name.get()), 0, arg->size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, nullptr, 0);
  if (addr == -1) return MAP_FAILED;

  // printf("addr=%p\n", addr);

  auto region = curproc->mm->lookup(addr);
  if (region == nullptr) {
    return MAP_FAILED;
  }

  region->obj = obj;

  // printf("[mshare] get  '%s' %p\n", name.get(), obj.get());
  // curproc->mm->dump();

  return (void *)addr;
}


unsigned long sys::mshare(int action, void *arg) {
  switch (action) {
    case MSHARE_CREATE:
      if (!VALIDATE_RD(arg, sizeof(struct mshare_create))) return -1;
      return (unsigned long)msh_create((struct mshare_create *)arg);

    case MSHARE_ACQUIRE:
      if (!VALIDATE_RDWR(arg, sizeof(struct mshare_acquire))) return -1;
      return (unsigned long)msh_acquire((struct mshare_acquire *)arg);
  }

  return -1;
}
