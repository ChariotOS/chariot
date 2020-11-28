#pragma once

#define CK_EVENT_READ 1
#define CK_EVENT_WRITE 2
#define CK_EVENT_CLOSE 3
// everything else is free to be used!

namespace ck {
  struct event {
    unsigned long type;
    // Different event implementations extend this structure and add fields :^)
  };
}  // namespace ck
