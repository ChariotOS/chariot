// A single header file for all things task and scheduler related

#pragma once

#include <func.h>
#include <task.h>


// forward declare
class thread;
class process;

namespace sched {

bool init(void);

bool enabled();

#define PRIORITY_IDLE 1
#define PRIOIRTY_LOW 10
#define PRIORITY_NORMAL 25
#define PRIORITY_HIGH 100

struct create_opts {
  int timeslice = 1;
  int priority = PRIORITY_NORMAL;
};

process &kernel_proc(void);

// return the process relating to the pid_t, and null if there is no process at
// that pid
process *process_pid(pid_t);

pid_t spawn_process(const char *name);
pid_t next_pid(void);

thread *spawn_kernel_thread(const char *name, func<void(int)>,
                            create_opts opts = {});

void yield(void);

void block();

// does not return
void run(void);

void handle_tick(u64 tick);

// force the process to exit, (yield with different state)
void exit();

void play_tone(int frq, int dur);

process &kernel_proc(void);

// make a 440hz beep for 100ms
void beep();
}  // namespace sched
