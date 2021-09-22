#ifndef wren_hpp
#define wren_hpp

// This is a convenience header for users that want to compile Wren as C and
// link to it from a C++ application.

#ifdef __cplusplus
extern "C" {
#endif
#include <wren/wren.h>
#ifdef __cplusplus
}
#endif

#endif
