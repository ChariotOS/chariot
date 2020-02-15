#include <cpu.h>
#include <fifo_buf.h>
#include <phys.h>
#include <sched.h>
#include <util.h>

// TODO: this will leak if one task uses a massive buffer
static spinlock fifo_block_cache_lock("fifo_block_cache_lock");
static struct fifo_block *fifo_block_cache = NULL;

struct fifo_block *fifo_block::alloc(void) {
  struct fifo_block *b = NULL;

  fifo_block_cache_lock.lock();
  if (fifo_block_cache != NULL) {
    // pull from the cache
    b = fifo_block_cache;
    fifo_block_cache = b->next;
  }
  fifo_block_cache_lock.unlock();

  if (b == NULL) b = (fifo_block *)phys::kalloc();

  // initialize the data
  b->next = NULL;
  b->prev = NULL;
  // b->len = PGSIZE - offsetof(struct fifo_block, data);
  b->len = 64;
  b->w = 0;
  b->r = 0;

  // zero out the buffer
  memset(b->data, 0x00, b->len);
  return b;
}

void fifo_block::free(struct fifo_block *b) {
  fifo_block_cache_lock.lock();

  // insert at the front of the queue
  b->next = fifo_block_cache;
  fifo_block_cache = b;

  fifo_block_cache_lock.unlock();
  // phys::kfree(b);
}

fifo_buf::fifo_buf(void) {}
fifo_buf::~fifo_buf(void) {
  while (write_block != NULL) {
    auto ob = write_block;
    write_block = ob->next;
    fifo_block::free(ob);
  }
}

void fifo_buf::wakeup_accessing_tasks(void) {}

void fifo_buf::block_accessing_tasks(void) {}

void fifo_buf::init_blocks() { write_block = read_block = fifo_block::alloc(); }

ssize_t fifo_buf::write(const void *vbuf, ssize_t size, bool block) {
  wlock.lock();

  if (write_block == NULL) init_blocks();
  auto *buf = (const char *)vbuf;

  size_t to_write = size;
  while (to_write > 0) {
    if (write_block->w >= write_block->len) {
      // make a new write block to work in
      auto nb = fifo_block::alloc();
      write_block->next = nb;
      nb->prev = write_block;
      write_block = nb;
    }

    size_t can_write = write_block->len - write_block->w;
    size_t will_write = min(to_write, can_write);

    memcpy(write_block->data + write_block->w, buf, will_write);

    // increment the write location
    write_block->w += will_write;
    to_write -= will_write;
    buf += will_write;
  }

  // record that there is data in the fifo
  navail += size;

  // possibly notify a reader (who will notify the next and so on)
  if (readers.should_notify(navail)) readers.notify();

  wlock.unlock();
  return size;
}

ssize_t fifo_buf::read(void *vbuf, ssize_t size, bool block) {
  rlock.lock();

  if (read_block == NULL) init_blocks();
  auto *buf = (char *)vbuf;

  // possibly block?
  if (navail < size && block) {
    rlock.unlock();
    int rude = readers.wait(size);
    if (rude) {
      if (readers.should_notify(navail)) readers.notify();
      return -1;
    }
    rlock.lock();
  }

  auto nread = 0;
  while (nread < size) {
    auto to_read = min(read_block->w - read_block->r, size - nread);

    if (to_read == 0) {
      if (block) {
        panic("fifo read (blocking) with missing data\n");
      }
      break;
    }

    // copy to the buffer
    memcpy(buf, read_block->data + read_block->r, to_read);

    // move our pointers
    read_block->r += to_read;
    buf += to_read;
    nread += to_read;

    // make sure we fix up the blocks when we use this one up
    if (read_block->r >= read_block->len) {
      // if we consumed the entire block, we can just reset this one for next
      // time :)
      if (read_block->next == NULL) {
        assert(read_block == write_block);
        read_block->w = 0;
        read_block->r = 0;
        memset(read_block->data, 0, read_block->len);
      } else {
        // we're at the end of the block, delete it
        assert(read_block->next != NULL);  // sanity check

        read_block = read_block->next;
        fifo_block::free(read_block->prev);
        read_block->prev = NULL;
      }
    }
  }

  navail -= nread;

  // sanity check
  assert(navail >= 0);

  // if more readers have built up, notify them with the new navail
  if (readers.should_notify(navail)) readers.notify();

  rlock.unlock();
  return nread;
}
