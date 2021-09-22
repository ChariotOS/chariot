#pragma once

#include <math.h>

namespace gfx {
  template <typename T>
  class vector4 final {
   public:
    constexpr vector4() = default;
    constexpr vector4(T x, T y, T z, T w) : m_x(x), m_y(y), m_z(z), m_w(w) {}

    constexpr T x() const { return m_x; }
    constexpr T y() const { return m_y; }
    constexpr T z() const { return m_z; }
    constexpr T w() const { return m_w; }

    constexpr void set_x(T value) { m_x = value; }
    constexpr void set_y(T value) { m_y = value; }
    constexpr void set_z(T value) { m_z = value; }
    constexpr void set_w(T value) { m_w = value; }

    constexpr vector4& operator+=(const vector4& other) {
      m_x += other.m_x;
      m_y += other.m_y;
      m_z += other.m_z;
      m_w += other.m_w;
      return *this;
    }

    constexpr vector4& operator-=(const vector4& other) {
      m_x -= other.m_x;
      m_y -= other.m_y;
      m_z -= other.m_z;
      m_w -= other.m_w;
      return *this;
    }

    constexpr vector4 operator+(const vector4& other) const {
      return vector4(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z, m_w + other.m_w);
    }

    constexpr vector4 operator-(const vector4& other) const {
      return vector4(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z, m_w - other.m_w);
    }

    constexpr vector4 operator*(const vector4& other) const {
      return vector4(m_x * other.m_x, m_y * other.m_y, m_z * other.m_z, m_w * other.m_w);
    }

    constexpr vector4 operator/(const vector4& other) const {
      return vector4(m_x / other.m_x, m_y / other.m_y, m_z / other.m_z, m_w / other.m_w);
    }

    constexpr vector4 operator*(T f) const { return vector4(m_x * f, m_y * f, m_z * f, m_w * f); }

    constexpr vector4 operator/(T f) const { return vector4(m_x / f, m_y / f, m_z / f, m_w / f); }

    constexpr T dot(const vector4& other) const {
      return m_x * other.m_x + m_y * other.m_y + m_z * other.m_z + m_w * other.m_w;
    }

    constexpr vector4 normalized() const {
      T inv_length = 1 / length();
      return *this * inv_length;
    }

    constexpr vector4 clamped(T m, T x) const {
      vector4 copy{*this};
      copy.clamp(m, x);
      return copy;
    }

    constexpr void clamp(T min_value, T max_value) {
      m_x = max(min_value, m_x);
      m_y = max(min_value, m_y);
      m_z = max(min_value, m_z);
      m_w = max(min_value, m_w);
      m_x = min(max_value, m_x);
      m_y = min(max_value, m_y);
      m_z = min(max_value, m_z);
      m_w = min(max_value, m_w);
    }

    constexpr void normalize() {
      T inv_length = 1 / length();
      m_x *= inv_length;
      m_y *= inv_length;
      m_z *= inv_length;
      m_w *= inv_length;
    }

    constexpr T length() const { return sqrt(m_x * m_x + m_y * m_y + m_z * m_z + m_w * m_w); }

   private:
    T m_x;
    T m_y;
    T m_z;
    T m_w;
  };

  typedef vector4<float> float4;
  typedef vector4<double> double4;

}  // namespace gfx
