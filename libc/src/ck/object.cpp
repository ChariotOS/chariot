#include <ck/object.h>
#include <stdio.h>



ck::object::object(void) {
	// printf("construct a '%s'\n", class_name());
}
ck::object::~object(void) {
	// printf("destruct a '%s'\n", class_name());
}
