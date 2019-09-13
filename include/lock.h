#pragma once

#include <asm.h>
#include <atom.h>

// TODO: use the correct cpu state
class mutex_lock {
 private:
  // compare and swap dest to spinlock on
  int locked = 0;

  // what CPU currently holds the lock?
  int holding_cpu = -1;

  const char *name;

 public:
  inline mutex_lock(const char *name) : name(name) {}

  void lock(void);
  void unlock(void);

  bool is_locked(void);
};

class scoped_lock {
  mutex_lock &lck;

 public:
  inline scoped_lock(mutex_lock &lck) : lck(lck) { lck.lock(); }

  inline ~scoped_lock(void) { lck.unlock(); }
};
