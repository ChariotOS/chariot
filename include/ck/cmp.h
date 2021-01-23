#pragma once

namespace ck {

  template <typename A, typename B>
  inline auto max(A a, B b) {
    return a > b ? a : b;
  }


  template <typename A, typename B>
  inline auto min(A a, B b) {
    return a < b ? a : b;
  }

};  // namespace ck
