#pragma once

#include "bitmap.h"
#include <ck/string.h>

namespace gfx {
	// load .bmp files
	ck::ref<gfx::bitmap> load_bmp(ck::string path);

	// load .png files
	ck::ref<gfx::bitmap> load_png(ck::string path);
};
