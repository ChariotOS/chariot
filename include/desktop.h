#pragma once

#include <types.h>
#include <lock.h>
#include <desktop_structs.h> // userspace api structs
#include <string.h>
#include <chan.h>

struct rect {
	// simple!
	int x, y, w, h;

	// methods for intersection logic
	// ... (use your imagination) ...
};

/**
 * every renderable *thing* is represented as a window in some kind
 * of global list in the window manager
 */
struct window {
	// every window can exist on any workspace. Think "mac control center"
	int workspace = 0;


	// the bounds of the window, including the window decorations. The content
	// position and size is derived from this.
	struct rect bounds;

	// pending userspace desktop events. This is what is read by the user.
	chan<struct desktop_window_event> pending;

	rwlock rw;

	window(struct rect bounds);
};
