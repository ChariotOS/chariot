#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <unistd.h>
#include <ck/io.h>

int main(int argc, char **argv) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		panic("what\n");
	}
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	// bind the local socket to the windowserver
	strncpy(addr.sun_path, "/usr/servers/lumen", sizeof(addr.sun_path)-1);

	int res = connect(fd, (const sockaddr *)&addr, sizeof(addr));
	if (res < 0) {
		fprintf(stderr, "Failed to connect\n");
		exit(EXIT_FAILURE);
	}


	// client
	const char *msg = "hello, world HOW ARE YOU?";
	int len = strlen(msg) + 1;
	write(fd, &len, sizeof(len));
	write(fd, msg, strlen(msg) + 1);

	char buf[512];
	int n = read(fd, buf, 512);
	if (n < 0) {
		return 0;
	}
	printf("userspace got %d bytes from connection\n", n);
	ck::hexdump(buf, n);

	close(fd);

	return 0;
}
