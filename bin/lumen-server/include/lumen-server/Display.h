#pragma once

#include <ui/surface.h>
#include <chariot/gvi.h>
#include <sys/ioctl.h>
#include <chariot/fb.h>

namespace server {
  class WindowManager;


  enum class FlushWithMemcpy {
    No = 0,
    Yes = 1,
  };


  // A display is a surface that can be used to render windows in the window manager
  class Display : public ui::surface {
   public:
    Display(WindowManager &wm);
    ~Display();


    // ^ui::surface
    virtual gfx::bitmap *bmp(void);
    virtual void invalidate(const gfx::rect &r);
    // screen resize doesnt work
    virtual ck::pair<int, int> resize(int width, int height);
    virtual ui::view *root_view(void);
    virtual void did_reflow(void);
    // ---------

    inline bool hardware_double_buffered(void) { return (info.caps & GVI_CAP_DOUBLE_BUFFER) != 0; }
    inline size_t screensize(void) { return bufsz; }
    inline uint32_t *buffer(void) {
      if (hardware_double_buffered()) return front_buffer;
      return back_buffer;
    }
    inline uint32_t *get_front_buffer_unsafe(void) { return front_buffer; }
    inline void set_pixel(int x, int y, uint32_t color) { buffer()[x + y * m_bounds.w] = color; }
    inline uint32_t get_pixel(int x, int y) { return buffer()[x + y * m_bounds.w]; }

    inline void clear(uint32_t color) {
      for (int i = 0; i < width() * height(); i++) {
        buffer()[i] = color;
      }
    }


    void flush_fb(FlushWithMemcpy with_memcpy = FlushWithMemcpy::No);

    inline int width(void) { return m_bounds.w; }
    inline int height(void) { return m_bounds.h; }

    inline const gfx::rect &bounds(void) { return m_bounds; }

   private:
    int fd = -1;
    size_t bufsz = 0;
    struct gvi_video_mode info;


    int buffer_index = 0;
    uint32_t *front_buffer;
    uint32_t *back_buffer;

    bool mouse_moved = false;

    gfx::rect m_bounds;
    gfx::point mouse_pos;

    WindowManager &wm;

    ck::ref<gfx::bitmap> m_bitmap;
  };
};  // namespace server