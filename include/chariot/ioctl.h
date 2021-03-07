#include "device_majors.h"
#define _IO(type, nr) ((unsigned int)((type & 0xFF) << 24) | (nr))

#define IOCTL_NUM(arg) ((arg)&0xFFFFFF)


#define PTMX_GETPTSID _IO(MAJOR_PTMX, 0)
