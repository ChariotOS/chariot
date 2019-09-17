#include <cpu.h>
#include <fifo_buf.h>
#include <sched.h>

inline void fifo_buf::compute_emptiness() {
  m_empty = m_read_buffer_index >= m_read_buffer->size() &&
            m_write_buffer->is_empty();
}

void fifo_buf::flip() {
  assert(m_read_buffer_index == m_read_buffer->size());
  swap(m_read_buffer, m_write_buffer);
  if (m_write_buffer->capacity() < 32)
    m_write_buffer->clear_with_capacity();
  else
    m_write_buffer->clear();
  m_read_buffer_index = 0;
  compute_emptiness();
}

ssize_t fifo_buf::write(const u8* data, ssize_t size) {

  // printk("write: %zd\n", size);
  if (size == 0) return 0;

  cpu::pushcli();
  m_lock.lock();

  navail += size;
  m_write_buffer->push(data, size);

  compute_emptiness();

  if (!waiters.is_empty()) {
    waiters.first().still_needs -= size;
    if (waiters.first().still_needs <= 0) {
      waiters.take_first().waiter->state = task::state::RUNNABLE;
    }
  }

  m_lock.unlock();
  cpu::popcli();
  return size;
}

ssize_t fifo_buf::read(u8* data, ssize_t size) {
  if (size == 0) return 0;


  cpu::pushcli();
  m_lock.lock();


  // printk("nvail = %d\n", navail);
  if (m_blocking && navail < size) {
    // printk("   append\n");
    waiters.append({.still_needs = size, .waiter = cpu::task()});

    m_lock.unlock();
    cpu::popcli();

    sched::block();


    cpu::pushcli();
    m_lock.lock();
  }

  if (m_read_buffer_index >= m_read_buffer->size() &&
      !m_write_buffer->is_empty()) {
    flip();
  }

  if (m_read_buffer_index >= m_read_buffer->size()) {
    m_lock.unlock();
    cpu::popcli();
    return 0;
  }

  ssize_t nread =
      min((ssize_t)m_read_buffer->size() - m_read_buffer_index, size);
  memcpy(data, m_read_buffer->data() + m_read_buffer_index, nread);
  m_read_buffer_index += nread;
  compute_emptiness();

  navail -= nread;
  assert(navail >= 0);

  m_lock.unlock();
  cpu::popcli();
  return nread;
}
