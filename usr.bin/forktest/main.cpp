#include <sys/syscall.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ck/io.h>

int fork(void) {
	return syscall(SYS_fork);
}

int fib(int n) {
	if (n < 2) return n;

	int a = 0;
	int b = 0;


	int pid1 = 0;
	int pid2 = 0;

	if ((pid1 = fork()) == 0) {
		syscall(SYS_exit_proc, fib(n - 1));
	}
	waitpid(pid1, &a, 0);

	if ((pid2 = fork()) == 0) {
		syscall(SYS_exit_proc, fib(n - 2));
	}
	waitpid(pid2, &b, 0);

	return WEXITSTATUS(a) + WEXITSTATUS(b);
}

int main() {
	printf("fork fib: %d\n", fib(10));

	return 0;
}
