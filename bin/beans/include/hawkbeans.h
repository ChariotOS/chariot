/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2019, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#pragma once

#define HAWKBEANS_VERSION_STRING "0.3.0"

// These surround string literals with the ANSI escape codes to render text with
// color on supporting terminal emulators
#define COLORED(color, str) "\e[" #color "m" str "\e[39m"
#define BLACK(str) COLORED(30, str)
#define RED(str) COLORED(31, str)
#define GREEN(str) COLORED(32, str)
#define YELLOW(str) COLORED(33, str)
#define BLUE(str) COLORED(34, str)
#define MAGENTA(str) COLORED(35, str)
#define CYAN(str) COLORED(36, str)
#define WHITE(str) COLORED(37, str)
#define BRBLACK(str) COLORED(90, str)
#define BRRED(str) COLORED(91, str)
#define BGREEN(str) COLORED(92, str)
#define BRYELLOW(str) COLORED(93, str)
#define BRBLUE(str) COLORED(94, str)
#define BRMAGENTA(str) COLORED(95, str)
#define BRCYAN(str) COLORED(96, str)
#define BRWHTIE(str) COLORED(97, str)

// Surrounds a string literal with the ANSI escape codes to make it render as
// bold in a supporting terminal emulator
#define BOLD(str) "\e[1m" str "\e[22m"

// "Escapes" boldness in a bolded string
#define UNBOLD(str) "\e[22m" str "\e[1m"

/* TODO: this should be arch specific */
#include <stdio.h>
#define HB_INFO(fmt, args...) fprintf(stderr, fmt "\n", ##args)
#define HB_INFO_NOBRK(fmt, args...) fprintf(stderr, fmt, ##args)
#define HB_ERR(fmt, args...) fprintf(stderr, BOLD(RED(fmt)) "\n", ##args)
#define HB_WARN(fmt, args...) fprintf(stderr, BOLD(YELLOW("WARNING: " fmt)) "\n", ##args)

#define HB_SUGGEST(fmt, args...) fprintf(stderr, BOLD(fmt) "\n", ##args)

#define HB_SUGGEST_NOBRK(fmt, args...) fprintf(stderr, BOLD(fmt), ##args)

#ifdef DEBUG_ENABLE
#define HB_DEBUG(fmt, args...) fprintf(stderr, BOLD(YELLOW("[" __FILE__ ":%d] " fmt)) "\n", __LINE__, ##args)
#else
#undef HB_DEBUG
#define HB_DEBUG(fmt, args...)
#endif
