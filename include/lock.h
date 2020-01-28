#pragma once

#include <asm.h>
#include <atom.h>

// TODO: use the correct cpu state
class spinlock {
 private:
  // compare and swap dest to spinlock on
  int locked = 0;

  int owner_tid = -1;
  const char *name = NULL;

 public:
  inline spinlock(const char *name = "unnamed") : name(name) { locked = 0; }

  void lock(void);
  void unlock(void);

  // for just locking ints
  static void lock(volatile int &);
  static void unlock(volatile int &);
  bool is_locked(void);
};

class rwlock {
 public:
  int rlock();
  int runlock();

  int wlock();
  int wunlock();

 private:
  spinlock m_lock = spinlock("rwlock.m_lock");
  unsigned m_readers = 0;
};

class scoped_lock {
  spinlock &lck;

 public:
  inline scoped_lock(spinlock &lck) : lck(lck) { lck.lock(); }

  inline ~scoped_lock(void) { lck.unlock(); }
};

