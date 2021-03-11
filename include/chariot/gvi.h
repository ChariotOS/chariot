#pragma once

#include "./ioctl.h"

struct gvi_video_mode {
  /* The capabilities of this video device as a bitmap of
   * GVI_CAP_* fields
   */
  unsigned long caps;
  unsigned int width;
  unsigned int height;
};

#define GVI_GET_MODE _IOR(MAJOR_VIDEO, 0, struct gvi_video_mode)
#define GVI_SET_MODE _IOW(MAJOR_VIDEO, 1, struct gvi_video_mode)
#define GVI_FLUSH_FB _IO(MAJOR_VIDEO, 2)
/* Return GVI_IDENTIFY_MAGIC */
#define GVI_IDENTIFY _IO(MAJOR_VIDEO, 3)

/* GVI Capabilities bitmap fields */
#define GVI_CAP_DOUBLE_BUFFER (1 << 0)


/* Just some random value */
#define GVI_IDENTIFY_MAGIC (0xb5939486)

// Only do this in the kernelspace
#ifdef KERNEL



#endif
