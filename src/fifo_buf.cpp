#include <cpu.h>
#include <fifo_buf.h>
#include <sched.h>

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
