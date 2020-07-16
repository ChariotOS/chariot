#pragma once

/**
 * Kernel API for controlling event polling from userspace
 *
 * A typical flow for even management is as follows:
 * ```
 * // open the event multiplexer so we can get an event manager
 * int emx = open("/dev/emx", O_RDONLY);
 * int ev = ioctl(emx, EMX_NEW, 0);
 * close(emx)
 *
 * struct evctl_setcmd to_set {
 *   .fd = my_fd,
 *   .flags = flags, // what are we waiting on?
 *   .data = my_data, // what data is returned on EMX_WAIT's return
 * };
 *
 * ioctl(ev, EMX_SET, &to_set);
 *
 *
 * struct evctl_waitcmd to_wait {
 *		.timeout = -1;
 * };
 * int res = ioctl(ev, EMX_WAIT, &to_wait); // wait with no timeout
 * // handle errors with res... did we timeout?
 *
 * // get the userspace data associated with the event that occurred.
 * // we need to do this because the event manager does not bind state
 * // the process and therefore does not understand the concept of a "file
 * // descriptor" according to userspace's understanding
 * void *data = to_wait.data;
 *
 * close(ev);
 * ```
 */

/* Api for /dev/emx (event multiplex) */

// create a new event file. Event files are just normal file descriptors
// but have ioctl's for controlling and waiting on events.
#define EMX_NEW 0

/* API on the event file */

// add a file descriptor to the thread's event set
#define EMX_SET 1
// remove a file descriptor from the thread's event set
#define EMX_REM 2
// wait for an event with a possible timeout
#define EMX_WAIT 3


#ifdef __cplusplus
extern "C" {
#endif

// available flags
#define EMX_READ (1 << 0)
#define EMX_WRITE (1 << 1)
#define EMX_ANY (0xFFFFFFFF)

struct emx_set_args {
	int fd;
	void *key;
	int events;
};

struct emx_wait_args {
	// in
	long long timeout;
	// out
	void *key;
	int events;
};


#ifdef __cplusplus
}
#endif
