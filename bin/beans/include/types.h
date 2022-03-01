/* 
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2017, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the 
 * file "LICENSE.txt".
 */
#pragma once

#include <stdint.h>

typedef int64_t   i8;
typedef int32_t   i4;
typedef int16_t   i2;
typedef uint64_t  u8;
typedef uint32_t  u4;
typedef uint16_t  u2;
typedef uint8_t   u1;
typedef float     f4;
typedef double    d8;

typedef uint8_t Bool;
#define bool Bool

#define false 0
#define true 1

#define HB_NULL (0)

#define T_BOOLEAN 4
#define T_CHAR 5
#define T_FLOAT	6
#define T_DOUBLE 7
#define T_BYTE 8
#define T_SHORT 9
#define T_INT 10
#define T_LONG 11
#define T_REF 12
