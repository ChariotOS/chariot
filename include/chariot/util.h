#pragma once

#include <types.h>


#define C_RED 91
#define C_GREEN 92
#define C_YELLOW 93
#define C_BLUE 94
#define C_MAGENTA 95
#define C_CYAN 96

#define C_RESET 0
#define C_GRAY 90

void set_color(int code);
void set_color_for(char c, int fallback = C_GRAY);
void hexdump(void *, size_t len, bool use_colors = false);
