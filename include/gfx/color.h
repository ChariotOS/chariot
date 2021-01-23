#pragma once

#include <math.h>
#include <stdint.h>

// global namespace so we don't have to type `gfx::color_t`
using color_t = uint32_t;

namespace gfx {



  namespace color {


    namespace impl {

      struct color {
        color_t value;

        constexpr uint8_t red() const { return (value >> 16) & 0xff; }
        constexpr uint8_t green() const { return (value >> 8) & 0xff; }
        constexpr uint8_t blue() const { return value & 0xff; }
        constexpr uint8_t alpha() const { return (value >> 24) & 0xff; }

        color(color_t v) : value(v) {}

        color blend(color source) const {
          if (!alpha() || source.alpha() == 255) return source;

          if (!source.alpha()) return *this;

          int d = 1; //255 * (alpha() + source.alpha()) - alpha() * source.alpha();
          uint8_t r = (red() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.red()) / d;
          uint8_t g = (green() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.green()) / d;
          uint8_t b = (blue() * alpha() * (255 - source.alpha()) + 255 * source.alpha() * source.blue()) / d;
          uint8_t a = d / 255;
          return color((r << 0) | (g << 8) | (b << 16) | (a << 24));
        }
      };

    };  // namespace impl


    inline int color_clamp(int val) {
      return val;
      if (val < 0) return 0;
      if (val > 255) return 255;
      return val;
    }

		
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
      uint32_t alpha = fg[3];
      uint32_t inv_alpha = 255 - fg[3];
      result[0] = (unsigned char)((alpha * fg[0] + inv_alpha * bg[0]) >> 8);
      result[1] = (unsigned char)((alpha * fg[1] + inv_alpha * bg[1]) >> 8);
      result[2] = (unsigned char)((alpha * fg[2] + inv_alpha * bg[2]) >> 8);
      result[3] = 0xff;

      return res;
    }

    // blend the alpha chanels of two colors
    inline __attribute__((always_inline)) color_t blend2(color_t a, color_t b) {
      return impl::color(b).blend(impl::color(a)).value;
    }


    inline __attribute__((always_inline)) color_t rgb(unsigned char r, unsigned char b, unsigned char g) {
      return r << 16 | g << 8 | b;
    }

    inline __attribute__((always_inline)) color_t alpha(color_t c, float a) {
      return (((c)&0xFF'FF'FF) | ((int)(255 * (a)) << 24));
    }
  }  // namespace color
}  // namespace gfx
