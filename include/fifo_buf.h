#pragma once

#include <lock.h>
#include <mem.h>
#include <single_list.h>
#include <vec.h>
#include <wait.h>

struct fifo_block {
  struct fifo_block *next, *prev;
  // atomic lock
  int lck = 0;
  u16 r, w, len;
  char pad[6];
  char data[];  // at the end of the struct

  // ease of use functions
  inline void lock() { mutex::lock(lck); }
  inline void unlock() { mutex::unlock(lck); }

  static struct fifo_block *alloc(void);
  static void free(struct fifo_block *);
};

class fifo_buf {
 public:
  fifo_buf();
  ~fifo_buf();

  ssize_t write(const void *, ssize_t, bool block = false);
  ssize_t read(void *, ssize_t, bool block = true);

  inline int size(void) const { return navail; }

 private:
  void wakeup_accessing_tasks(void);
  void block_accessing_tasks(void);

  void init_blocks();

  bool m_blocking;
  spinlock wlock;
  spinlock rlock;

  fifo_block *read_block;
  fifo_block *write_block;

  ssize_t navail = 0;

  waitqueue readers;
};
