#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * framebuffer magic is done through a single "info" structure.
 * which lets you control resolution, status, etc...
 *
 * All through ioctl to the framebuffer driver
 */

#define FB_SET_INFO 1
#define FB_GET_INFO 2
#define FB_SET_XOFF 3
#define FB_SET_YOFF 4

struct ck_fb_info {
  int active;
  unsigned long width;
  unsigned long height;
};

#ifdef __cplusplus
}  // extern "C"
#endif
