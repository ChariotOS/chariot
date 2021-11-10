#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/sysbind.h>

int main(int argc, char **argv) {
	return sysbind_kshell();
}
