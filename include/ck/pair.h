#pragma once
#include <ck/utility.h>


namespace ck {

  /// two element tuple
  template <typename T1, typename T2>
  struct pair {
   public:
    T1 first;
    T2 second;

    constexpr pair(const pair&) = default;       ///< Copy constructor
    constexpr pair(pair&&) = default;            ///< Move constructor
    constexpr pair(void) : first(), second() {}  ///< Default constructor

    /// Constructor accepting lvalues of T1 and T2
    constexpr pair(const T1& x, const T2& y)
        : first(ck::forward<T1>(x)), second(ck::forward<T2>(y)) {}
    /// Constructor accepting two values of arbitrary types
    template <typename U1, typename U2>
    constexpr pair(U1&& x, U2&& y) : first(ck::forward<T1>(x)), second(ck::forward<T2>(y)) {}

    /// Converting Constructor from pair<U1, U2> lvalue
    template <typename U1, typename U2>
    constexpr pair(const pair<U1, U2>& p)
        : first(ck::forward<T1>(p.first)), second(ck::forward<T2>(p.second)) {}

    /// Converting Constructor from pair<U1, U2> rvalue
    template <typename U1, typename U2>
    constexpr pair(pair<U1, U2>&& p)
        : first(ck::forward<T1>(p.first)), second(ck::forward<T2>(p.second)) {}


    ~pair(void) = default;  ///< default destructor

    /// Copy assignment operator
    constexpr pair& operator=(const pair& p) {
      first = p.first;
      second = p.second;
      return *this;
    }

    /// Move assignment operator
    constexpr pair& operator=(pair&& p) {
      first = ck::forward<T1>(p.first);
      second = ck::forward<T2>(p.second);
      return *this;
    }
  };
}  // namespace ck
