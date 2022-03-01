#pragma once

#include <hawkbeans.h>
#include <thread.h>

#define PROMPT_STR GREEN(BOLD("(hawkbeans-shell)$> "))

void run_shell (jthread_t * cpu, bool interactive);
