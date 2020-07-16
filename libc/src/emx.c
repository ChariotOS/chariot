#include <sys/emx.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>


int emx_create(void) {
	int host = open("/dev/emx", O_RDONLY);

	int emx = ioctl(host, EMX_NEW, 0);

	close(host);

	return emx;
}

int emx_set(int emx, int fd, void *key, int events) {
	struct emx_set_args args;

	args.fd = fd;
	args.key = key;
	args.events = events;

	return ioctl(emx, EMX_SET, &args);
}

int emx_rem(int emx, void *key) {
	return ioctl(emx, EMX_REM, key);
}


void *emx_wait(int emx, int *events) {
	// -1 indicates no timeout
	return emx_wait_timeout(emx, events, -1);
}

void *emx_wait_timeout(int emx, int *events, time_t timeout) {


	struct emx_wait_args args;
	args.timeout = timeout;

	int res = ioctl(emx, EMX_WAIT, &args);
	if (res < 0) return NULL;

	if (events != NULL) *events = args.events;

	return args.key;
}
