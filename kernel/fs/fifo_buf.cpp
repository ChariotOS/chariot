#include <cpu.h>
#include <errno.h>
#include <fifo_buf.h>
#include <phys.h>
#include <sched.h>
#include <util.h>


size_t fifo_buf::available(void) {
  if (read_ptr == write_ptr) {
    return m_size - 1;
  }

  if (read_ptr > write_ptr) {
    return read_ptr - write_ptr - 1;
  } else {
    return (m_size - write_ptr) + read_ptr - 1;
  }
}


size_t fifo_buf::unread(void) {
  if (read_ptr == write_ptr) {
    return 0;
  }
  if (read_ptr > write_ptr) {
    return (m_size - read_ptr) + write_ptr;
  } else {
    return (write_ptr - read_ptr);
  }
}



void fifo_buf::increment_read(void) {
  read_ptr++;
  if (read_ptr == m_size) {
    read_ptr = 0;
  }
}

void fifo_buf::increment_write(void) {
  write_ptr++;
  if (write_ptr == m_size) {
    write_ptr = 0;
  }
}



void fifo_buf::close(void) {
  m_closed = true;

  wq_readers.wake_up_all();
  wq_writers.wake_up_all();
}


void fifo_buf::init_if_needed(void) {
  if (buffer == NULL) {
    m_size = 4096 * 16;  // 16 pages
    buffer = (char *)kmalloc(m_size);
  }
}


ssize_t fifo_buf::write(const void *vbuf, ssize_t size, bool block) {
  init_if_needed();

  auto ibuf = (char *)vbuf;

  size_t written = 0;
  while (written < size) {
    lock.lock_cli();

    while (available() > 0 && written < size) {
      buffer[write_ptr] = ibuf[written];
      increment_write();
      written++;
    }


    // printk_nolock("available: %d\n", available());

    lock.unlock_cli();
    wq_readers.wake_up_all();

    if (!block) return -EAGAIN;

    if (written < size) {
      if (wq_writers.wait().interrupted()) {
        if (written > 0) return written;
        return -EINTR;
      }
      if (m_closed) return -ECONNRESET;
    }
  }

  return written;
}


ssize_t fifo_buf::read(void *vbuf, ssize_t size, bool block) {
  init_if_needed();


  if (!block) {
    scoped_lock l(lock);

    if (unread() == 0) {
      if (m_closed) return 0;
      return -EAGAIN;
    }
  }


  auto obuf = (char *)vbuf;

  size_t collected = 0;
  while (collected == 0) {
    lock.lock_cli();

    while (unread() > 0 && collected < size) {
      obuf[collected] = buffer[read_ptr];
      increment_read();
      collected++;
    }


    // printk_nolock("unread: %d\n", unread());

    lock.unlock_cli();
    wq_writers.wake_up_all();


    // break early if we are not meant to block
    if (!block) {
      break;
    }

    if (collected == 0) {
      if (wq_readers.wait().interrupted()) {
        // We haven't read anything anyways, just return that there was an
        // interrupt due to signals.
        return -EINTR;
      }
    }
  }


  return collected;
}

void fifo_buf::stats(size_t &avail, size_t &unread) {
  scoped_lock l(lock);

  avail = this->available();
  unread = this->unread();
}



int fifo_buf::poll(void) {
  // upon close, it returns AWAITFS_READ (idk, its expected)
  if (m_closed) return AWAITFS_READ;

  int ev = 0;
  lock.lock_cli();
  if (unread() > 0) {
    ev |= AWAITFS_READ;
  }

  if (available() > 0) {
    ev |= AWAITFS_WRITE;
  }
  lock.unlock_cli();
  return ev;
}

