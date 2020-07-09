#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>

int main() {
	int parent = getpid();
	printf("parent: %d\n", parent);
	volatile int a = 30;

	int pid = syscall(SYS_fork);

	if (pid == 0) {
		a = 42;
		printf("I am the child. %d is the parent. a=%d\n", parent, a);
		exit(0);
	}

	// waitpid(pid, NULL, 0);
	printf("I am the parent. %d is the child. a=%d\n", pid, a);


	return 0;
}
