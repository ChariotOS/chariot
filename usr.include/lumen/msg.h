#pragma once

#include <stdlib.h>


#define LUMEN_MAGIC 0x4e4d554c // 'L' 'U' 'M' 'N'

namespace lumen {

#define LUMEN_MSG_PING 0
#define LUMEN_MSG_PONG 1

	struct msg {
		int magic;
		int type;

		// length of the payload
		int len;

		// "unique" id for this message. That way we know who to notify
		// when a response is gotten.
		unsigned long id;

		char data[];
	};



}
