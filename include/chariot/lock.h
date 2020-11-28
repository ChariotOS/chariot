#pragma once

#include <asm.h>
#include <atom.h>


#define WARN_UNUSED __attribute__((warn_unused_result))

// TODO: use the correct cpu state
class spinlock {
 private:
  // compare and swap dest to spinlock on
  int locked = 0;
 public:
  inline spinlock() { locked = 0; }

  void lock(void);
  void unlock(void);


	void lock_cli();
	void unlock_cli();

  // for just locking ints
  static void lock(volatile int &);
  static void unlock(volatile int &);
  bool is_locked(void);
};

class rwlock {
 public:
  int read_lock();
  int read_unlock();

  int write_lock();
  int write_unlock();

 private:
  spinlock m_lock;
  unsigned m_readers = 0;
};

class scoped_lock {
  spinlock &lck;

 public:
  inline scoped_lock(spinlock &lck) : lck(lck) { lck.lock(); }

  inline ~scoped_lock(void) { lck.unlock(); }
};

