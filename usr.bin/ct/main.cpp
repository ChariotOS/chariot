#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <lumen.h>
#include <ck/io.h>
#include <ck/socket.h>

#include <ck/eventloop.h>

int main(int argc, char **argv) {


	// ck::eventloop loop;
	lumen::session session;

	if (!session.connected()) {
		fprintf(stderr, "failed to connect\n");
		exit(EXIT_FAILURE);
	}
	while (true) {
		char c = getchar();
		if (c == '\n') break;
		session.send_msg(3, c);
	}

	// loop.start();

	return 0;
}
