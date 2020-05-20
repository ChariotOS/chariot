#include "loadbmp.h"

#include <fs.h>
#include <fs/vfs.h>
#include <mem.h>
#include <syscall.h>

unsigned int load_bmp(const char *filename, int *&imageData,
		int &width, int &height,
		unsigned int components) {
	auto f = vfs::fdopen(filename, O_RDONLY, 0);

	if (!f) return LOADBMP_FILE_NOT_FOUND;

	unsigned char bmp_file_header[14];
	unsigned char bmp_info_header[40];
	unsigned char bmp_pad[3];

	unsigned int w, h;
	unsigned char *data = NULL;

	unsigned int x, y, i, padding;

	memset(bmp_file_header, 0, sizeof(bmp_file_header));
	memset(bmp_info_header, 0, sizeof(bmp_info_header));

	if (f.read(bmp_file_header, sizeof(bmp_file_header)) == 0) {
		// close file desc
		return LOADBMP_INVALID_FILE_FORMAT;
	}

	if (f.read(bmp_info_header, sizeof(bmp_info_header)) == 0) {
		// close file desc
		return LOADBMP_INVALID_FILE_FORMAT;
	}

	if ((bmp_file_header[0] != 'B') || (bmp_file_header[1] != 'M')) {
		// close file desc
		return LOADBMP_INVALID_SIGNATURE;
	}

	if ((bmp_info_header[14] != 24) && (bmp_info_header[14] != 32)) {
		// close file desc
		return LOADBMP_INVALID_BITS_PER_PIXEL;
	}

	w = (bmp_info_header[4] + (bmp_info_header[5] << 8) +
			(bmp_info_header[6] << 16) + (bmp_info_header[7] << 24));
	h = (bmp_info_header[8] + (bmp_info_header[9] << 8) +
			(bmp_info_header[10] << 16) + (bmp_info_header[11] << 24));

	if ((w > 0) && (h > 0)) {
		data = (unsigned char *)kmalloc(w * h * components);

		if (!data) {
			// close file desc
			return LOADBMP_OUT_OF_MEMORY;
		}

		for (y = (h - 1); y != -1; y--) {
			for (x = 0; x < w; x++) {
				i = (x + y * w) * components;

				if (f.read(data + i, 3) == 0) {
					kfree(data);
					// close file desc
					return LOADBMP_INVALID_FILE_FORMAT;
				}

				data[i] ^= data[i + 2] ^= data[i] ^= data[i + 2];  // BGR -> RGB

				if (components == LOADBMP_RGBA) data[i + 3] = 255;
			}

			padding = ((4 - (w * 3) % 4) % 4);

			if (f.read(bmp_pad, padding) != padding) {
				kfree(data);
				// close file desc
				return LOADBMP_INVALID_FILE_FORMAT;
			}
		}
	}

	width = w;
	height = h;
	imageData = (int*)data;

	// close file desc

	return LOADBMP_NO_ERROR;
}

