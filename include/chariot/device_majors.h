#pragma once
/**
 * This file is the primary location where major numbers for drivers are
 * defined, and it exists to avoid overlapping major numbers for different
 * drivers
 *
 * A major number is a simple 16 bit number that represents a driver's identity.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	MAJOR_NULL,
	MAJOR_MEM,
	MAJOR_ATA,
	MAJOR_DISK,

	MAJOR_COM,

	// PS2
	MAJOR_MOUSE,
	MAJOR_KEYBOARD,

	// console
	MAJOR_CONSOLE,

	// VGA framebuffer
	MAJOR_FB,

	// Soundblaster 16
	MAJOR_SB16,

	// Pseudoterminal
	MAJOR_PTMX,
	MAJOR_PTS,

} major_numbers;


#ifdef __cplusplus
}
#endif

