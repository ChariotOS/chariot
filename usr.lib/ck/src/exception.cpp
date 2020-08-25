#include <stdlib.h>
#include <stdio.h>

namespace std {
	struct type_info;
}
/*

extern "C" void* __cxa_allocate_exception(size_t thrown_size) throw() {
	return malloc(thrown_size);
}


extern "C" void __cxa_free_exception(void *e) throw() {
	free(e);
}


extern "C" void __cxa_throw(void* thrown_exception, struct std::type_info * tinfo, void (*dest)(void*)) {
	printf("cxa_throw: ex: %p, tinfo: %p, dest: %p", thrown_exception, tinfo, dest);

}



extern "C" void* __cxa_begin_catch(void* exceptionObject) throw() {
	printf("cxa_begin_catch: %p\n", exceptionObject);
	exit(-1);
	return NULL;
}


extern "C" void __cxa_end_catch(void) {}
*/
