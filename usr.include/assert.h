
#pragma once

#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef __cplusplus
extern "C" {
#endif



#ifdef NDEBUG
#define	assert(x) (void)0
#else
#define assert(x) ((void)((x) || (__assert_fail(#x, __FILE__, __LINE__, __func__),0)))
#endif


extern void __assert_fail(const char * assertion, const char * file, unsigned int line, const char * function);

#ifdef __cplusplus
}
#endif

#endif
