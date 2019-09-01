#pragma once
/**
 * This file is the primary location where major numbers for drivers are
 * defined, and it exists to avoid overlapping major numbers for different
 * drivers
 *
 * A major number is a simple 16 bit number that represents a driver's identity.
 */


// MEM driver - for access to physical memory as a device
#define MAJOR_MEM 1

// ATA drives
#define MAJOR_ATA 4
