#pragma once

struct mountopts {
	char device[64]; /* The device name */
	char dir[256]; /* The directory to mount it in */
	char type[64]; /* The kind of filesystem */
	unsigned long flags;
	void *data; /* impl by the filesystem */
};
