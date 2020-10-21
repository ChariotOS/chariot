#pragma once

#include <lock.h>
#include <mem.h>
#include <single_list.h>
#include <vec.h>
#include <wait.h>
#include <awaitfs.h>

class fifo_buf {
 public:
  inline fifo_buf() {
		init_if_needed();
	}
  inline ~fifo_buf() {
		if (buffer != NULL) kfree(buffer);
	}

  ssize_t write(const void *, ssize_t, bool block = false);
  ssize_t read(void *, ssize_t, bool block = true);
  inline int size(void) { return unread(); }
	void close();
	int poll();
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

	char * buffer;
	size_t write_ptr = 0;
	size_t read_ptr = 0;
	size_t m_size;

	wait::queue wq_readers;
	wait::queue wq_writers;


	size_t available(void);
	size_t unread(void);

	void increment_read(void);
	void increment_write(void);
};
