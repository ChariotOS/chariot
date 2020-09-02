#define _GNU_SOURCE
#include <dlfcn.h>


// this is all placeholder!
int dladdr(void* a, Dl_info* b) { return 0; }

void *dlopen(const char *filename, int flags) {
	return 0;
}
void *dlsym(void *handle, const char *symbol) {
	return 0;
}

int dlclose(void *handle) {
	return 0;
}
