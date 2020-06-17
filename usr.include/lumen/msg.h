#pragma once

#include <stdlib.h>


#define LUMEN_MAGIC 0x5D4c

namespace lumen {

  // messages with F set at the top are designed to be sent to the window
  // server, not the client handler
#define FOR_WINDOW_SERVER 0xF000

  // first message that is to be sent
#define LUMEN_MSG_GREET (0 | FOR_WINDOW_SERVER)
#define LUMEN_MSG_GREETBACK (1)



  struct msg {
    unsigned short magic;
    unsigned short type;

    // length of the payload
    int len;

    // "unique" id for this message. That way we know who to notify
    // when a response is gotten.
    unsigned long id;

    char data[];
  };



  struct greet_msg {
    int pid;  // the pid of the process
  };

#define LUMEN_GREETBACK_MAGIC 0xF00D
	struct greetback_msg {
		unsigned short magic;
		int client_id;
	};
}  // namespace lumen
