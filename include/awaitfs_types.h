#pragma once



#ifdef __cplusplus
extern "C" {
#endif


#define AWAITFS_READ (1 << 0)
#define AWAITFS_WRITE (1 << 1)

// #define AWAITFS_

// wait for any event :)
#define AWAITFS_ALL 0xFFFF

struct await_target {
  int fd;
  short awaiting;  // what events you request
  short occurred;  // what events occurred
  void *priv;      // private data, useful to the user :^)
};


#ifdef __cplusplus
}
#endif
