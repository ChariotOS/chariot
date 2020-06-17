#pragma once

#include <stdlib.h>


#define LUMEN_MAGIC 0x5D4c

namespace lumen {

#define LUMEN_MSG_PING 0
#define LUMEN_MSG_PONG 1

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
}
