#pragma once

#include <list_head.h>
#include <types.h>
#include <wait.h>

struct sleep_blocker {
	struct sleep_blocker *prev;
	struct sleep_blocker *next;
  wait_queue wq;
  uint64_t wakeup_us;
};

/* returns 0 or -ERRNO */
int do_usleep(uint64_t us);
void check_wakeups(void);
