#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <stdint.h>

namespace gfx {


  class bitmap : public ck::object {
   public:
    static ck::ref<gfx::bitmap> create_shared(size_t w, size_t h);
    static ck::ref<gfx::bitmap> create(size_t w, size_t h);
    bitmap(size_t w, size_t h, bool shared = false);
    ~bitmap(void);

    inline uint32_t get_pixel(uint32_t x, uint32_t y) const {
      if (x < 0 || x >= m_width) return 0;
      if (y < 0 || y >= m_height) return 0;
      return m_pixels[x + y * m_width];
    }

    inline void set_pixel(uint32_t x, uint32_t y, uint32_t color) {
      if (x < 0 || x >= m_width) return;
      if (y < 0 || y >= m_height) return;
      m_pixels[x + y * m_width] = color;
    }

    inline size_t size(void) { return m_width * m_height * sizeof(uint32_t); }

    inline size_t width(void) { return m_width; }
    inline size_t height(void) { return m_height; }

   private:
    size_t m_width, m_height;
    bool m_shared = false;
    uint32_t *m_pixels = NULL;
  };
};  // namespace gfx
