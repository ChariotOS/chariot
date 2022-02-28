#pragma once


#include <wait.h>


struct poll_table_wait_entry {
  struct wait_entry entry;
  wait_queue *wq;
  struct poll_table *table;
  short events;
  int index;
  bool en; /* idk im trying my best. */
  /* Was this the entry that was awoken? */
  bool awoken = false;
};


struct poll_table {
  ck::vec<poll_table_wait_entry *> ents;
  int index;
  void wait(wait_queue &wq, short events);
};
