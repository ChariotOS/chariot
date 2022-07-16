#pragma once



#ifdef __cplusplus
extern "C" {
#endif

#define KCTL_MAX_NAMELEN 64

#include "kctl.inc"

#define KCTL_IS_NAME(name) (((name) & KCTL_NAME_MASK) != 0)


enum kctl_name {
#define __KCTL(number, name, NAME) kctl_ ## name = number,
  ENUMERATE_KCTL_NAMES
#undef __KCTL
};


#ifdef __cplusplus
}
#endif
