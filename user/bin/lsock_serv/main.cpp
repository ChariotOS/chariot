#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char **argv) {
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	printf("fd=%d\n", fd);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;

	// bind the local socket globally
	strncpy(addr.sun_path, "lsock_test", sizeof(addr.sun_path)-1);
	int bind_res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
	printf("bind res: %d\n", bind_res);



	return 0;
}
