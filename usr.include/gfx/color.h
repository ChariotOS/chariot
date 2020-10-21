#pragma once

#include <stdint.h>


// global namespace so we don't have to type `gfx::color_t`
using color_t = uint32_t;

namespace gfx {


  namespace color {

    // blend the alpha chanels of two colors
    inline __attribute__((always_inline)) color_t blend(color_t a, color_t b) {
      // only blend if we have to!
      if ((a & 0xFF000000) >> 24 == 0xFF) return a;
      if ((a & 0xFF000000) >> 24 == 0x00) return b;

      color_t res = 0;
      auto result = (unsigned char *)&res;
      auto fg = (unsigned char *)&a;
      auto bg = (unsigned char *)&b;

      // spooky math follows
      uint32_t alpha = fg[3] + 1;
      uint32_t inv_alpha = 256 - fg[3];
      result[0] = (unsigned char)((alpha * fg[0] + inv_alpha * bg[0]) >> 8);
      result[1] = (unsigned char)((alpha * fg[1] + inv_alpha * bg[1]) >> 8);
      result[2] = (unsigned char)((alpha * fg[2] + inv_alpha * bg[2]) >> 8);
      result[3] = 0xff;

      return res;
    }


		inline __attribute__((always_inline)) color_t rgb(unsigned char r, unsigned char b, unsigned char g) {
			return r << 16 | g << 8 | b;
		}

    inline __attribute__((always_inline)) color_t alpha(color_t c, float a) {
      return (((c)&0xFF'FF'FF) | ((int)(255 * (a)) << 24));
    }
  }  // namespace color
}  // namespace gfx
