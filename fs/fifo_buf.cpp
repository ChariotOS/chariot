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

  wq_readers.notify_all();
  wq_writers.notify_all();
}


void fifo_buf::init_if_needed(void) {
  if (buffer == NULL) {
    m_size = 4096;
    buffer = (char *)kmalloc(m_size);
  }
}


ssize_t fifo_buf::write(const void *vbuf, ssize_t size, bool block) {
  init_if_needed();

  auto ibuf = (char *)vbuf;

  size_t written = 0;
  while (written < size) {
    lock_write.lock();

    while (available() > 0 && written < size) {
      buffer[write_ptr] = ibuf[written];
      increment_write();
      written++;
    }


    wq_readers.notify_all();
    lock_write.unlock();

    if (!block) return -EAGAIN;

    if (written < size) {
      if (wq_writers.wait() == false /* interrupted by signals? */) {
        return -EINTR;
      }
      if (m_closed) return -ECONNRESET;
    }
  }

  return 0;
}


ssize_t fifo_buf::read(void *vbuf, ssize_t size, bool block) {
  init_if_needed();


  if (!block) {
    scoped_lock l(lock_read);

    if (unread() == 0) {
      if (m_closed) return 0;
      return -EAGAIN;
    }
  }


  auto obuf = (char *)vbuf;

  size_t collected = 0;
  while (collected == 0) {
    lock_read.lock();

    while (unread() > 0 && collected < size) {
      obuf[collected] = buffer[read_ptr];
      increment_read();
      collected++;
    }

    wq_writers.notify_all();
    lock_read.unlock();

    // break early if we are not meant to block
    if (!block) {
      break;
    }

    if (collected == 0) {
      if (wq_readers.wait() == false /* Were we interrupted?? */) {
        // We haven't read anything anyways, just return that there was an
        // interrupt due to signals.
        return -EINTR;
      }
    }
  }


  return collected;
}


int fifo_buf::poll(void) {
  // upon close, it returns AWAITFS_READ (idk, its expected)
  if (m_closed) return AWAITFS_READ;

  int ev = 0;
  lock_read.lock();
  if (unread() > 0) {
    ev |= AWAITFS_READ;
  }
  lock_read.unlock();


  lock_write.lock();
  if (available() > 0) {
    ev |= AWAITFS_WRITE;
  }
  lock_write.unlock();
  return ev;
}
