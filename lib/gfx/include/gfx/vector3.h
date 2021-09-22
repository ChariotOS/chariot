#pragma once

#include <math.h>

namespace gfx {
  template <typename T>
  class vector3 final {
   public:
    constexpr vector3() = default;
    constexpr vector3(T x, T y, T z) : m_x(x), m_y(y), m_z(z) {}

    constexpr T x() const { return m_x; }
    constexpr T y() const { return m_y; }
    constexpr T z() const { return m_z; }

    constexpr void set_x(T value) { m_x = value; }
    constexpr void set_y(T value) { m_y = value; }
    constexpr void set_z(T value) { m_z = value; }

    constexpr vector3& operator+=(const vector3& other) {
      m_x += other.m_x;
      m_y += other.m_y;
      m_z += other.m_z;
      return *this;
    }

    constexpr vector3& operator-=(const vector3& other) {
      m_x -= other.m_x;
      m_y -= other.m_y;
      m_z -= other.m_z;
      return *this;
    }

    constexpr vector3 operator+(const vector3& other) const {
      return vector3(m_x + other.m_x, m_y + other.m_y, m_z + other.m_z);
    }

    constexpr vector3 operator-(const vector3& other) const {
      return vector3(m_x - other.m_x, m_y - other.m_y, m_z - other.m_z);
    }

    constexpr vector3 operator*(const vector3& other) const {
      return vector3(m_x * other.m_x, m_y * other.m_y, m_z * other.m_z);
    }

    constexpr vector3 operator/(const vector3& other) const {
      return vector3(m_x / other.m_x, m_y / other.m_y, m_z / other.m_z);
    }

    constexpr vector3 operator*(T f) const { return vector3(m_x * f, m_y * f, m_z * f); }

    constexpr vector3 operator/(T f) const { return vector3(m_x / f, m_y / f, m_z / f); }

    constexpr T dot(const vector3& other) const {
      return m_x * other.m_x + m_y * other.m_y + m_z * other.m_z;
    }

    constexpr vector3 cross(const vector3& other) const {
      return vector3(m_y * other.m_z - m_z * other.m_y, m_z * other.m_x - m_x * other.m_z,
          m_x * other.m_y - m_y * other.m_x);
    }

    constexpr vector3 normalized() const {
      T inv_length = 1 / length();
      return *this * inv_length;
    }

    constexpr vector3 clamped(T m, T x) const {
      vector3 copy{*this};
      copy.clamp(m, x);
      return copy;
    }

    constexpr void clamp(T min_value, T max_value) {
      m_x = MAX(min_value, m_x);
      m_y = MAX(min_value, m_y);
      m_z = MAX(min_value, m_z);
      m_x = MIN(max_value, m_x);
      m_y = MIN(max_value, m_y);
      m_z = MIN(max_value, m_z);
    }

    constexpr void normalize() {
      T inv_length = 1 / length();
      m_x *= inv_length;
      m_y *= inv_length;
      m_z *= inv_length;
    }

    constexpr T length() const { return sqrt(m_x * m_x + m_y * m_y + m_z * m_z); }
    constexpr vector3 pow(T v) { return vector3(powf(x(), v), powf(y(), v), powf(z(), v)); }

   private:
    T m_x;
    T m_y;
    T m_z;
  };

  typedef vector3<float> float3;
  typedef vector3<double> double3;

}  // namespace gfx