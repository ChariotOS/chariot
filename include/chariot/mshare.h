#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define MSHARE_CREATE 1
#define MSHARE_ACQUIRE 2

#define MSHARE_NAMESZ 128

struct mshare_create {
	unsigned long size; // the size of the region
	char name[MSHARE_NAMESZ]; // eventual name of the region
};

struct mshare_acquire {
  unsigned long size;  // How much of the region to map (can be over-mapped)
	char name[MSHARE_NAMESZ]; // the name of the region to acquire
};



#ifdef __cplusplus
}
#endif

