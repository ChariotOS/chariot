#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <gfx/rect.h>
#include <gfx/point.h>
#include <stdint.h>

namespace gfx {

  class bitmap : public ck::refcounted<bitmap> {
    // CK_OBJECT(gfx::bitmap);
    CK_NONCOPYABLE(bitmap);
    CK_MAKE_NONMOVABLE(bitmap);

   public:
    bitmap(size_t w, size_t h);
		// create with an unowned buffer (not freed)
    bitmap(size_t w, size_t h, uint32_t *buffer);
    virtual ~bitmap(void);

    inline uint32_t get_pixel(uint32_t x, uint32_t y) const {
      if (x < 0 || x >= m_width) return 0;
      if (y < 0 || y >= m_height) return 0;
      return m_pixels[x + y * m_width];
    }

    inline void set_pixel(int x, int y, uint32_t color) {
      if (x < 0 || x > (int)m_width) return;
      if (y < 0 || y > (int)m_height) return;
      m_pixels[x + y * m_width] = color;
    }

		inline uint32_t *pixels(void) {return m_pixels;}
		inline uint32_t *scanline(uint32_t y) {return  m_pixels + (y * m_width); }

    inline size_t size(void) { return m_width * m_height * sizeof(uint32_t); }

    inline size_t width(void) const { return m_width; }
    inline size_t height(void) const { return m_height; }

    inline void clear(uint32_t c) {
      for (size_t i = 0; i < width() * height(); i++) m_pixels[i] = c;
    }



		inline gfx::rect rect(void) const {
			return gfx::rect(0, 0, width(), height());
		}


   protected:
    bitmap(){};  // protected constructor
    size_t m_width, m_height;
		bool m_owned = true;
    uint32_t *m_pixels = NULL;
  };



  class shared_bitmap : public gfx::bitmap {
    // CK_OBJECT(gfx::shared_bitmap);
    CK_NONCOPYABLE(shared_bitmap);
    CK_MAKE_NONMOVABLE(shared_bitmap);

   public:
   public:
    inline const char *shared_name(void) const { return m_name.get(); }

    static ck::ref<gfx::shared_bitmap> get(const char *name, size_t w,
                                           size_t h);
    shared_bitmap(size_t w, size_t h);

    virtual ~shared_bitmap(void);


    // applies no transformation to the image. Just gets a new canvas
    ck::ref<gfx::shared_bitmap> resize(size_t w, size_t h);

    // DO NOT USE THIS
    shared_bitmap(const char *name, uint32_t *, size_t w, size_t h);

   protected:
    size_t m_original_size;
    const ck::string m_name;
  };

};  // namespace gfx
