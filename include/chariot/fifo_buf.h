#pragma once

#include <lock.h>
#include <mem.h>
#include <ck/single_list.h>
#include <ck/vec.h>
#include <wait.h>
#include <awaitfs.h>
#include <fs.h>

class fifo_buf {
 public:
  inline fifo_buf() { init_if_needed(); }
  inline ~fifo_buf() {
    if (buffer != NULL) free(buffer);
  }

  ssize_t write(const void *, ssize_t, bool block = false);
  ssize_t read(void *, ssize_t, bool block = true, long long timeout_us = -1);
  inline int size(void) { return unread(); }


  inline size_t buffer_size(void) { return m_size; }
  void close();
  int poll(poll_table &pt);
  void stats(size_t &avail, size_t &unread);

 private:
  void init_if_needed(void);

  char *get_buffer(void);

  bool m_closed = false;
  bool m_blocking;


  spinlock lock;

  /*
spinlock lock_write;
spinlock lock_read;
  */

  char *buffer;
  size_t write_ptr = 0;
  size_t read_ptr = 0;
  size_t m_size;

  struct wait_queue wq_readers;
  struct wait_queue wq_writers;


  size_t available(void);
  size_t unread(void);

  void increment_read(void);
  void increment_write(void);
};
