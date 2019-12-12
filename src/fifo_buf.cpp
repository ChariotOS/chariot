#include <cpu.h>
#include <fifo_buf.h>
#include <sched.h>
#include <phys.h>

struct fifo_block *fifo_block::alloc(void) {
  auto b = (fifo_block *)phys::kalloc();

  // initialize the data
  b->len = PGSIZE - offsetof(struct fifo_block, data);

  memset(b->data, 0xFF, b->len);

  return b;
}
void fifo_block::free(struct fifo_block *b) {
  phys::kfree(b);
}



void fifo_buf::wakeup_accessing_tasks(void) {
  cpu::pushcli();

  cpu::popcli();
}

void fifo_buf::block_accessing_tasks(void) {
  cpu::pushcli();

  /*
  for (auto &a : accessing_threads) {
    a.waiter->
  }

    List_Foreach (n, pipe->accessingThreads)
    {
        Thread* reader = n->data;

        reader->state = TS_WAITIO;

        reader->state_privateData = pipe;
    }
    */
  cpu::popcli();
  halt();
}

ssize_t fifo_buf::write(const void* data, ssize_t size) { return 0; }

ssize_t fifo_buf::read(void* data, ssize_t size) { return 0; }
