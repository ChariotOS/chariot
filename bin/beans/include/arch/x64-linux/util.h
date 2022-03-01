#pragma once

#include <types.h>

static inline u1
get_u1 (u1 * src)
{
	return *src;
}

static inline u2
get_u2 (u1 * src)
{
	u1 fst = *src;
	u2 sec = *(src+1);
	return ((u2)fst << 8) | sec;
}

static inline u4
get_u4 (u1 * src)
{
	u1 fst = *src;
	u1 sec = *(src+1);
	u1 thr = *(src+2);
	u1 fou = *(src+3);
	
	return ((u4)fst << 24) |
               ((u4)sec << 16) |
	       ((u4)thr <<  8) |
	       ((u4)fou);
}

static inline u8
get_u8 (u1 * src)
{
	u1 fst = *src;
	u1 sec = *(src+1);
	u1 thr = *(src+2);
	u1 fou = *(src+3);
	u1 fif = *(src+4);
	u1 six = *(src+5);
	u1 sev = *(src+6);
	u1 eig = *(src+7);
	
	return ((u8)fst << 56) |
               ((u8)sec << 48) |
	       ((u8)thr << 40) |
	       ((u8)fou << 32) |
	       ((u8)fif << 24) |
	       ((u8)six << 16) |
	       ((u8)sev <<  8) |
	       ((u8)eig);
}
