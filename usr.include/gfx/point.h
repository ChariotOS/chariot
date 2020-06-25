#pragma once

#include <gfx/rect.h>


namespace gfx {


  class point {
   public:
    inline point() : m_x(0), m_y(0) {}


    inline void constrain(const gfx::rect &rect) {
      if (x() < rect.left())
        set_x(rect.left());
      else if (x() > rect.right())
        set_x(rect.right());
      if (y() < rect.top())
        set_y(rect.top());
      else if (y() > rect.bottom())
        set_y(rect.bottom());
    }

    inline int x(void) const { return m_x; }
    inline int y(void) const { return m_y; }
    inline void set_x(int x) { m_x = x; }
    inline void set_y(int y) { m_y = y; }

   private:
    int m_x, m_y;
  };
}  // namespace gfx
