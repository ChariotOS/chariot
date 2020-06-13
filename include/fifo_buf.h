#pragma once

#include <lock.h>
#include <mem.h>
#include <single_list.h>
#include <vec.h>
#include <wait.h>

class fifo_buf {
 public:
  inline fifo_buf() {
		init_if_needed();
		// printk("size: %zu, buff=%p\n", m_size, buffer);
	}
  inline ~fifo_buf() {
		kfree(buffer);
	}

  ssize_t write(const void *, ssize_t, bool block = false);
  ssize_t read(void *, ssize_t, bool block = true);

  inline int size(void) { return unread(); }

	void close();

 private:

	void init_if_needed(void);

	char *get_buffer(void);


	bool m_closed = false;
  bool m_blocking;


  spinlock lock_write;
  spinlock lock_read;

	char * buffer;
	size_t write_ptr = 0;
	size_t read_ptr = 0;
	size_t m_size;

  waitqueue wq_readers;
	waitqueue wq_writers;


	size_t available(void);
	size_t unread(void);

	void increment_read(void);
	void increment_write(void);
};
