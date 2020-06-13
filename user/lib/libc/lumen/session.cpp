#include <lumen.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <ck/io.h>


lumen::session::session(void) {
	fd = socket(AF_UNIX, SOCK_STREAM, 0);



	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	// bind the local socket to the windowserver
	strncpy(addr.sun_path, "/usr/servers/lumen", sizeof(addr.sun_path)-1);

	connect(fd, (const sockaddr *)&addr, sizeof(addr));
}

lumen::session::~session(void) {
	// TODO: send the shutdown message

	if (fd != -1) {
		close(fd);
	}
}


static unsigned long nextmsgid(void) {
	static unsigned long sid = 0;


	return sid++;
}

long lumen::session::send_raw(int type, void *payload, size_t payloadsize) {
	size_t msgsize = payloadsize + sizeof(lumen::msg);
	auto msg = (lumen::msg *)malloc(msgsize);

	msg->type = type;
	msg->id = nextmsgid();
	msg->len = payloadsize;

	if (payloadsize > 0) {
		memcpy(msg + 1, payload, payloadsize);
	}

	auto w = write(fd, (const void*)msg, msgsize);

	free(msg);

	return w;
}


