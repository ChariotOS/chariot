#pragma once
// failure state
#define MAP_FAILED ((void *) -1)

// TODO: add the other MAP_* operators from libc
#define MAP_SHARED     0x01
#define MAP_PRIVATE    0x02
#define MAP_ANON       0x20
#define MAP_ANONYMOUS  MAP_ANON

#define PROT_NONE      0
#define PROT_READ      1
#define PROT_WRITE     2
#define PROT_EXEC      4
#define PROT_GROWSDOWN 0x01000000
#define PROT_GROWSUP   0x02000000


struct mmap_region {

	// a unique id
	int id;

	// offset and length
	unsigned long long off, len;

	// MAP_*
	int flags;

	// PROT_*
	int prot;

	// the region's name
	char name[64];
};
