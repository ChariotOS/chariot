#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <lumen.h>
#include <ck/io.h>

int main(int argc, char **argv) {

	lumen::session session;
	while (true) {
		char c = getchar();
		if (c == '!') break;
		session.send_msg(3, c);
	}

	return 0;
}

