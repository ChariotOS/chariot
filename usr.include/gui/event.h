#pragma once

#include <stdint.h>
#include <lumen/msg.h>

namespace gui {
	struct mouse_event {
		int x, y, dx, dy, buttons;
	};
}
