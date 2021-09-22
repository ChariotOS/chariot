#pragma once

#include <gfx/direction.h>

namespace gfx {
  template <typename T>
  struct size {
   public:
    size() = default;
    size(T w, T h) : m_width(w), m_height(h) {
    }

    template <typename U>
    size(U width, U height) : m_width(width), m_height(height) {
    }

    template <typename U>
    explicit size(const size<U>& other) : m_width(other.width()), m_height(other.height()) {
    }
    [[nodiscard]] T width() const {
      return m_width;
    }
    [[nodiscard]] T height() const {
      return m_height;
    }
    [[nodiscard]] T area() const {
      return width() * height();
    }



    void set_width(T w) {
      m_width = w;
    }
    void set_height(T h) {
      m_height = h;
    }

    [[nodiscard]] bool is_null() const {
      return !m_width && !m_height;
    }
    [[nodiscard]] bool is_empty() const {
      return m_width <= 0 || m_height <= 0;
    }

    void scale_by(T dx, T dy) {
      m_width *= dx;
      m_height *= dy;
    }

    void scale_by(T dboth) {
      scale_by(dboth, dboth);
    }


    template <class U>
    bool operator==(const size<U>& other) const {
      return width() == other.width() && height() == other.height();
    }

    template <class U>
    bool operator!=(const size<U>& other) const {
      return !(*this == other);
    }

    size<T>& operator-=(const size<T>& other) {
      m_width -= other.m_width;
      m_height -= other.m_height;
      return *this;
    }

    size<T>& operator+=(const size<T>& other) {
      m_width += other.m_width;
      m_height += other.m_height;
      return *this;
    }

    size<T> operator*(T factor) const {
      return {m_width * factor, m_height * factor};
    }

    size<T>& operator*=(T factor) {
      m_width *= factor;
      m_height *= factor;
      return *this;
    }


    T primary_size_for_direction(gfx::Direction direction) const {
      return direction == gfx::Direction::Vertical ? height() : width();
    }

    void set_primary_size_for_direction(gfx::Direction direction, T value) {
      if (direction == gfx::Direction::Vertical) {
        set_height(value);
      } else {
        set_width(value);
      }
    }

    T secondary_size_for_direction(gfx::Direction direction) const {
      return direction == gfx::Direction::Vertical ? width() : height();
    }

    void set_secondary_size_for_direction(gfx::Direction direction, T value) {
      if (direction == gfx::Direction::Vertical) {
        set_width(value);
      } else {
        set_height(value);
      }
    }


   private:
    T m_width = 0;
    T m_height = 0;
  };



  using isize = size<int>;
  using fsize = size<float>;
};  // namespace gfx
