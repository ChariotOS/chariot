#pragma once

#include <gfx/rect.h>
namespace gfx {
  void stackblur(uint32_t *src,        ///< input image data
                 unsigned int w,       ///< image width
                 unsigned int h,       ///< image height
                 unsigned int radius,  ///< blur intensity (should be in 2..254 range)
                 const gfx::rect &bounds);
}
