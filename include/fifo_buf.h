#pragma once

#include <lock.h>
#include <mem.h>
#include <single_list.h>
#include <vec.h>

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
  inline fifo_buf(int cap = 4096, bool blocking = false)
      : m_blocking(blocking), m_lock("fifo") {
    capacity = cap;
    data = (u8 *)kmalloc(cap);
  }

  inline ~fifo_buf() { kfree(data); }

  ssize_t write(const void *, ssize_t);
  ssize_t read(void *, ssize_t);

  inline int size(void) const { return navail; }

 private:
  void wakeup_accessing_tasks(void);
  void block_accessing_tasks(void);

  bool m_blocking;
  int navail = 0;

  single_list<struct task *> accessing_threads;

  u8 *data;
  u32 write_index;
  u32 read_index;
  u32 capacity;
  u32 used_bytes;
  mutex_lock m_lock;
};
