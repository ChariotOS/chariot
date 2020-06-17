#include "internal.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>

lumen::screen::screen(int w, int h) {
	fd = open("/dev/fb", O_RDWR);
	set_resolution(w, h);
}

lumen::screen::~screen(void) {
	munmap(buf, bufsz);
	close(fd);
	buf = NULL;
}

void lumen::screen::set_resolution(int w, int h) {
		if (buf != NULL) {
			munmap(buf, bufsz);
		}


		info.width = w;
		info.height = h;
		flush_info();

		bufsz = w * h * sizeof(uint32_t);
		buf = (uint32_t*)mmap(NULL, bufsz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
}


