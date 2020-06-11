#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <chariot/fb.h>
#include <sys/ioctl.h>



struct vga_framebuffer {


	vga_framebuffer(int w, int h) {
		fd = open("/dev/fb", O_RDWR);
		set_resolution(w, h);
	}

	~vga_framebuffer(void) {
		munmap(buf, bufsz);
		close(fd);
		buf = NULL;
	}

	void set_resolution(int w, int h) {
		if (buf != NULL) {
			munmap(buf, bufsz);
		}


		info.width = w;
		info.height = h;
		flush_info();

		bufsz = w * h * sizeof(uint32_t);
		buf = (uint32_t*)mmap(NULL, bufsz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	}

	uint32_t *pixels(void) {
		return buf;
	}

	private:
		int fd = -1;
		uint32_t *buf = NULL;
		size_t bufsz = 0;
		struct ck_fb_info info;

		void flush_info(void) {
			info.active = 1;
			ioctl(fd, FB_SET_INFO, &info);
		}

		void load_info(void) {
			ioctl(fd, FB_GET_INFO, &info);
		}

};

// static storage, which we write to
volatile int x = 32;

int main(int argc, char **argv) {

	printf("before write\n");
	x += 1;
	printf("after write\n");
	printf("x=%d\n", x);


	int fd = open("/small", O_RDWR);
	auto buf = (char*)mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	buf[0] = '1';

	close(fd);

	/*
	vga_framebuffer fb(1024, 768);


	fb.pixels()[0] = 0xFF00FF;

	(void)getchar();
	*/

	return 0;

	/*

	ck::file file("/small", "rw");

	file.seek(2 * 4096, SEEK_END);

	int o = 0;
#define COL  30
#define ROW 20
	for (int i = 0; i < ROW * COL; i++) {
		file.writef("%02x ", i & 0xFF);
		o++;


		if (o == COL) {
			file.writef("\n");
			o = 0;
		}
	}
	file.writef("\n");

	return 0;
	*/
}
