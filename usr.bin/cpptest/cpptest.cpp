#include <stdio.h>
#include <stdlib.h>
#include <ck/io.h>


int main() {
	void *p = malloc(100);
	ck::hexdump((void*)((off_t)p - 200), 300);

  return 0;
}
