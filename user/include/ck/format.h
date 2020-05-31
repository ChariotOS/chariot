#pragma once

#include <ck/string.h>

/**
 * ck::format(T) is an api that most cpp types in ck:: should follow
 * that converts some type into a string
 */

// FORMAT is a wrapper around ck::format that is usable in the
// bog-standard printf(...) function in C. (ie: char* values)
#define FORMAT(thing) (ck::format(thing).get())

namespace ck {
	/*
	ck::string format(char);
	ck::string format(unsigned char);

	ck::string format(int);
	ck::string format(unsigned int);
	ck::string format(long);
	ck::string format(unsigned long);
	*/
}
