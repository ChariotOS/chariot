#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>


int main(int argc, char **argv) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	printf("client fd: %d\n", fd);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	// bind the local socket to the windowserver
	strncpy(addr.sun_path, "/usr/servers/windowserver", sizeof(addr.sun_path)-1);

	int res = connect(fd, (const sockaddr *)&addr, sizeof(addr));

	printf("connect: %d\n", res);


	return 0;
}
