#include "internal.h"


lumen::window::window(int id, lumen::guest &g, int w, int h) : id(id), guest(g), rect(0, 0, w, h) {
	printf("w: %d, h: %d\n", w, h);
	bitmap = gfx::shared_bitmap::create(rect.w, rect.h);
}
