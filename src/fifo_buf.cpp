#include <cpu.h>
#include <fifo_buf.h>
#include <phys.h>
#include <sched.h>


// TODO: this will leak if one task uses a massive buffer
static mutex_lock fifo_block_cache_lock;
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
  b->len = PGSIZE - offsetof(struct fifo_block, data);
  b->w = 0;
  b->r = 0;

  // zero out the buffer
  memset(b->data, 0xFF, b->len);
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

void fifo_buf::wakeup_accessing_tasks(void) {}

void fifo_buf::block_accessing_tasks(void) {}

ssize_t fifo_buf::write(const void *data, ssize_t size) { return 0; }

ssize_t fifo_buf::read(void *data, ssize_t size) { return 0; }
