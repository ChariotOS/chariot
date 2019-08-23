#pragma once

#include <asm.h>



// TODO: use the correct cpu state

class mutex_lock {
  private:

    // compare and swap dest to spinlock on
    u32 locked;

    // name of the lock
    const char *name;

    // what CPU currently holds the lock?
    int holding_cpu;
  public:
    mutex_lock(const char * name = "generic_lock");


    void lock(void);
    void unlock(void);

    bool is_locked(void);
};
