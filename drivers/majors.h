#pragma once
/**
 * This file is the primary location where major numbers for drivers are
 * defined, and it exists to avoid overlapping major numbers for different
 * drivers
 *
 * A major number is a simple 16 bit number that represents a driver's identity.
 */

#define MAJOR_NULL 0

// MEM driver - for access to physical memory as a device
#define MAJOR_MEM 1


// ATA drives
#define MAJOR_ATA 4

#define MAJOR_MOUSE 10
#define MAJOR_KEYBOARD 11

#define MAJOR_CONSOLE 20

#define MAJOR_FB 21

#define MAJOR_SB16 22
