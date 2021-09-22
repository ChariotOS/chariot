#pragma once

#include <chariot/termios.h>
#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif


int tcgetattr(int fd, struct termios*);
int tcsetattr(int fd, int optional_actions, const struct termios*);
// int tcflow(int fd, int action);
// int tcflush(int fd, int queue_selector);

int tcsetpgrp(int fd, pid_t pgrp);
pid_t tcgetpgrp(int fd);


#ifdef __cplusplus
}
#endif
