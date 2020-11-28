#pragma once

#define PUBLISH_ATTR(attr)\
	inline decltype(attr) get_##attr(void) { return this->attr; } \
	inline void set_##attr(decltype(attr) v) { this->attr = v; } \


