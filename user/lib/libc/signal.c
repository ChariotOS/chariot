#include <signal.h>

void (*signal(int sig, void (*func)(int)))(int) { return SIG_ERR; }
