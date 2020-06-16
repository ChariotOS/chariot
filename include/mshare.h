#pragma once

#ifdef __cplusplus
extern "C" {
#endif


#define MSHARE_PUBLISH 1
#define MSHARE_ACQUIRE 2
#define MSHARE_RELEASE 3


struct mshare_publish {
  void *addr;
};

struct mshare_acquire {
  unsigned long id;     // The ID of the region you wish to acquire
  unsigned long *size;  // filled in by the systemcall
};

struct mshare_release {
  void *address;  // the start of the region
};

#ifdef __cplusplus
}
#endif

