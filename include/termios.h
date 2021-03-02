#pragma once

#include <chariot/termios.h>
#include <unistd.h>


#ifdef __cplusplus
extern "C" {
#endif


int tcsetpgrp(int fd, pid_t pgrp);
pid_t tcgetpgrp(int fd);


#ifdef __cplusplus
}
#endif
