#include <sys/ioctl.h>
#include <stdarg.h>
#include <sys/sysbind.h>

int ioctl(int fd, int req, ...)
{
	void *arg;
	va_list ap;
	va_start(ap, req);
	arg = va_arg(ap, void *);
	va_end(ap);
	return sysbind_ioctl(fd, req, (unsigned long)arg);
}
