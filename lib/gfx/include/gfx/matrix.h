
#pragma once

#include <stdlib.h>
#include <std.h>
#include <assert.h>
#include <gfx/vector3.h>
#include <gfx/vector4.h>


namespace gfx {

  template <size_t N, typename T>
  class matrix {
   public:
    static constexpr size_t Size = N;

    constexpr matrix() = default;
    constexpr matrix(std::initializer_list<T> elements) {
      assert(elements.size() == N * N);
      size_t i = 0;
      for (auto& element : elements) {
        m_elements[i / N][i % N] = element;
        ++i;
      }
    }

    template <typename... Args>
    constexpr matrix(Args... args) : matrix({(T)args...}) {}

    matrix(const matrix& other) {
      __builtin_memcpy(m_elements, other.elements(), sizeof(T) * N * N);
    }

    constexpr auto elements() const { return m_elements; }
    constexpr auto elements() { return m_elements; }

    constexpr matrix operator*(const matrix& other) const {
      matrix product;
      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          auto& element = product.m_elements[i][j];

          if constexpr (N == 4) {
            element = m_elements[i][0] * other.m_elements[0][j] +
                      m_elements[i][1] * other.m_elements[1][j] +
                      m_elements[i][2] * other.m_elements[2][j] +
                      m_elements[i][3] * other.m_elements[3][j];
          } else if constexpr (N == 3) {
            element = m_elements[i][0] * other.m_elements[0][j] +
                      m_elements[i][1] * other.m_elements[1][j] +
                      m_elements[i][2] * other.m_elements[2][j];
          } else if constexpr (N == 2) {
            element = m_elements[i][0] * other.m_elements[0][j] +
                      m_elements[i][1] * other.m_elements[1][j];
          } else if constexpr (N == 1) {
            element = m_elements[i][0] * other.m_elements[0][j];
          } else {
            T value{};
            for (size_t k = 0; k < N; ++k)
              value += m_elements[i][k] * other.m_elements[k][j];

            element = value;
          }
        }
      }

      return product;
    }

    constexpr matrix operator/(T divisor) const {
      matrix division;
      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          division.m_elements[i][j] = m_elements[i][j] / divisor;
        }
      }
      return division;
    }

    constexpr matrix adjugate() const {
      if constexpr (N == 1) return matrix(1);

      matrix adjugate;
      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          int sign = (i + j) % 2 == 0 ? 1 : -1;
          adjugate.m_elements[j][i] = sign * first_minor(i, j);
        }
      }
      return adjugate;
    }

    constexpr T determinant() const {
      if constexpr (N == 1) {
        return m_elements[0][0];
      } else {
        T result = {};
        int sign = 1;
        for (size_t j = 0; j < N; ++j) {
          result += sign * m_elements[0][j] * first_minor(0, j);
          sign *= -1;
        }
        return result;
      }
    }

    constexpr T first_minor(size_t skip_row, size_t skip_column) const {
      static_assert(N > 1);
      assert(skip_row < N);
      assert(skip_column < N);

      matrix<N - 1, T> first_minor;
      constexpr auto new_size = N - 1;
      size_t k = 0;

      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          if (i == skip_row || j == skip_column) continue;

          first_minor.elements()[k / new_size][k % new_size] = m_elements[i][j];
          ++k;
        }
      }

      return first_minor.determinant();
    }

    constexpr static matrix identity() {
      matrix result;
      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          if (i == j)
            result.m_elements[i][j] = 1;
          else
            result.m_elements[i][j] = 0;
        }
      }
      return result;
    }

    constexpr matrix inverse() const {
      auto det = determinant();
      assert(det != 0);
      return adjugate() / det;
    }

    constexpr matrix transpose() const {
      matrix result;
      for (size_t i = 0; i < N; ++i) {
        for (size_t j = 0; j < N; ++j) {
          result.m_elements[i][j] = m_elements[j][i];
        }
      }
      return result;
    }

   private:
    T m_elements[N][N];
  };




  template <typename T>
  using matrix4x4 = matrix<4, T>;

  template <typename T>
  constexpr static vector4<T> operator*(const matrix4x4<T>& m, const vector4<T>& v) {
    auto const& elements = m.elements();
    return vector4<T>(v.x() * elements[0][0] + v.y() * elements[0][1] + v.z() * elements[0][2] +
                          v.w() * elements[0][3],
        v.x() * elements[1][0] + v.y() * elements[1][1] + v.z() * elements[1][2] +
            v.w() * elements[1][3],
        v.x() * elements[2][0] + v.y() * elements[2][1] + v.z() * elements[2][2] +
            v.w() * elements[2][3],
        v.x() * elements[3][0] + v.y() * elements[3][1] + v.z() * elements[3][2] +
            v.w() * elements[3][3]);
  }

  template <typename T>
  constexpr static vector3<T> transform_point(const matrix4x4<T>& m, const vector3<T>& p) {
    auto const& elements = m.elements();
    return vector3<T>(
        p.x() * elements[0][0] + p.y() * elements[0][1] + p.z() * elements[0][2] + elements[0][3],
        p.x() * elements[1][0] + p.y() * elements[1][1] + p.z() * elements[1][2] + elements[1][3],
        p.x() * elements[2][0] + p.y() * elements[2][1] + p.z() * elements[2][2] + elements[2][3]);
  }

  template <typename T>
  constexpr static vector3<T> transform_direction(const matrix4x4<T>& m, const vector3<T>& d) {
    auto const& elements = m.elements();
    return vector3<T>(d.x() * elements[0][0] + d.y() * elements[0][1] + d.z() * elements[0][2],
        d.x() * elements[1][0] + d.y() * elements[1][1] + d.z() * elements[1][2],
        d.x() * elements[2][0] + d.y() * elements[2][1] + d.z() * elements[2][2]);
  }

  template <typename T>
  constexpr static matrix4x4<T> translation_matrix(const vector3<T>& p) {
    return matrix4x4<T>(1, 0, 0, p.x(), 0, 1, 0, p.y(), 0, 0, 1, p.z(), 0, 0, 0, 1);
  }

  template <typename T>
  constexpr static matrix4x4<T> scale_matrix(const vector3<T>& s) {
    return matrix4x4<T>(s.x(), 0, 0, 0, 0, s.y(), 0, 0, 0, 0, s.z(), 0, 0, 0, 0, 1);
  }

  template <typename T>
  constexpr static matrix4x4<T> rotation_matrix(const vector3<T>& axis, T angle) {
    T c = cos(angle);
    T s = sin(angle);
    T t = 1 - c;
    T x = axis.x();
    T y = axis.y();
    T z = axis.z();

    return matrix4x4<T>(t * x * x + c, t * x * y - z * s, t * x * z + y * s, 0, t * x * y + z * s,
        t * y * y + c, t * y * z - x * s, 0, t * x * z - y * s, t * y * z + x * s, t * z * z + c, 0,
        0, 0, 0, 1);
  }

  typedef matrix4x4<float> float4x4;
  typedef matrix4x4<double> double4x4;


}  // namespace gfx