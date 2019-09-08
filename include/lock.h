#pragma once

#include <asm.h>
#include <atom.h>


// TODO: use the correct cpu state
class mutex_lock {
  private:

    // compare and swap dest to spinlock on
    u32 locked = 0;

    // what CPU currently holds the lock?
    int holding_cpu = -1;
  public:

    void lock(void);
    void unlock(void);

    bool is_locked(void);
};
