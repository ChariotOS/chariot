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


	// make an eventloop
	ck::eventloop loop;

	// connect to the "server"
	lumen::session session;

	// get a reference to stdin
	auto in = ck::file::unowned(0);
	// register a read handler
	in->on_read = [&] {
		char c = getchar();
		if (c == '\n') ck::eventloop::exit();
		session.send_msg(3, c);
	};

	// start the loop!
	loop.start();

	printf("\n");

	return 0;
}
