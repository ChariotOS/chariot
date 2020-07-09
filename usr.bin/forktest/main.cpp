#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

int main() {
	int parent = getpid();

	auto *ptr = (volatile int*)mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
	printf("ptr: %p\n", ptr);

	*ptr = 3;

	int pid = syscall(SYS_fork);



	if (pid == 0) {
		*ptr = 30;
		exit(0);
	}

	waitpid(pid, NULL, 0);
	printf("%d in parent\n", *ptr);

	return 0;
}
