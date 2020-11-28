#pragma once

#include <ck/func.h>

namespace ck {
  struct task_context {};

  template <typename T>
  struct future {
    T await(void);
  };




  template <typename T>
  ck::future<T> async(ck::func<T()> action) {
    while (1) {
    }
  }




}  // namespace ck
