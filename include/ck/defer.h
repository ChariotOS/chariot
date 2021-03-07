#pragma once

#include <ck/func.h>


// This is some crazy magic that helps produce __BASE__247
// Vanilla interpolation of __BASE__##__LINE__ would produce __BASE____LINE__
// I still can't figure out why it works, but it has to do with macro resolution ordering
#define PP_CAT(a, b) PP_CAT_I(a, b)
#define PP_CAT_I(a, b) PP_CAT_II(~, a##b)
#define PP_CAT_II(p, res) res

#define UNIQUE_NAME(base) PP_CAT(base, __COUNTER__)


#define defer(body) ck::__defer UNIQUE_NAME(_defer_)([&]() body)


namespace ck {

  // this struct is '''''private'''''
  class __defer {
    ck::func<void(void)> cb;

   public:
    inline __defer(ck::func<void(void)> cb) : cb(cb) {
    }

    inline ~__defer(void) {
      cb();
    }
  };
}  // namespace ck
