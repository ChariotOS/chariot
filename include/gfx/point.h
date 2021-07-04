#pragma once

#include <gfx/rect.h>


namespace gfx {


  template <typename T>
  class point_impl {
   public:
    inline point_impl() : m_x(0), m_y(0) {}
    inline point_impl(int x, int y) : m_x(x), m_y(y) {}


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

    inline point_impl<T> translated(T dx, T dy) const { return point_impl<T>(m_x + dx, m_y + dy); }

    inline int x(void) const { return m_x; }
    inline int y(void) const { return m_y; }
    inline void set_x(int x) { m_x = x; }
    inline void set_y(int y) { m_y = y; }


    inline point_impl operator+(const point_impl &other) {
      return point_impl<T>(m_x + other.x(), m_y + other.y());
    }


    inline point_impl operator-(const point_impl &other) {
      return point_impl<T>(m_x - other.x(), m_y - other.y());
    }

    inline point_impl operator*(const T &o) { return point_impl<T>(m_x * o, m_y * o); }

    template <typename V>
    point_impl<T> &operator=(const point_impl<V> &o) {
      m_x = o.x();
      m_y = o.y();
      return *this;
    }

   private:
    T m_x, m_y;
  };

  using point = gfx::point_impl<int>;
  using pointf = gfx::point_impl<float>;

  using float2 = gfx::point_impl<float>;
}  // namespace gfx
