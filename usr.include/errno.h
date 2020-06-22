#ifndef _ERRNO_H
#define _ERRNO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <chariot/errno.h>

#define __RETURN_WITH_ERRNO(rc, good_ret, bad_ret) \
  do {                                             \
    if (rc < 0) {                                  \
      errno = -rc;                                 \
      return (bad_ret);                            \
    }                                              \
    errno = 0;                                     \
    return (good_ret);                             \
  } while (0)

#ifdef __GNUC__
__attribute__((const))
#endif
int *__errno_location(void);
#define errno (*__errno_location())


#define WHILE_INTR(expr)      \
  ({                          \
    typeof(expr) _res;        \
    do {                      \
      _res = expr;           \
    } while (errno == EINTR); \
    _res;                     \
  })

#ifdef __cplusplus
}
#endif

#endif

