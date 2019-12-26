#pragma once

#include <asm.h>
#include <atom.h>

// TODO: use the correct cpu state
class spinlock {
 private:
  // compare and swap dest to spinlock on
  int locked = 0;

  // what CPU currently holds the lock?
  int holding_cpu = -1;

  const char *name;

 public:
  inline spinlock(const char *name) : name(name) {}
  inline spinlock() : name("generic_lock") {}

  void lock(void);
  void unlock(void);

  bool is_locked(void);
};

class scoped_lock {
  spinlock &lck;

 public:
  inline scoped_lock(spinlock &lck) : lck(lck) { lck.lock(); }

  inline ~scoped_lock(void) { lck.unlock(); }
};


// for POD structures
namespace mutex {
  void lock(int &);
  void unlock(int &);
};


