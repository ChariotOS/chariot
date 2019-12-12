#pragma once

#include <types.h>

#ifndef __CONSOLE_H
#define __CONSOLE_H

/**
 * the console is a way to focus user input and output into one location.
 * Many sources can send input to the console, and many places can read from the
 * console. For example, when the COM driver gets an input interrupt, it will
 * send it here
 */
namespace console {

#define CONS_DEL 0x7f

// feed bytes of input to the console. The reason this is a buffer of memory is
// for atomicity. While injesting input, the console is locked - this is so
// that, for example, multi-byte codes like arrow keys are registered atomically
void feed(size_t, char *);

// get a char from the console, optionally not blocking. returns -1 (int) when
// there is no char avail
int getc(bool block = false);

};  // namespace console

#endif
