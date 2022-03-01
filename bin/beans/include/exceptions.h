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

#include <types.h>
#include <hawkbeans.h>

#if DEBUG_EXCP == 1
#define EXCP_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define EXCP_DEBUG(fmt, args...)
#endif

#define EXCP_NULL_PTR     0
#define EXCP_IDX_OOB      1
#define EXCP_ARR_IDX_OOB  2
#define EXCP_INCMP_CLS_CH 3
#define EXCP_NEG_ARR_SIZE 4
#define EXCP_OOM          5
#define EXCP_CLS_NO_FOUND 6
#define EXCP_ARITH        7
#define EXCP_NO_FIELD     8
#define EXCP_NO_METHOD    9
#define EXCP_RUNTIME      10
#define EXCP_IO           11
#define EXCP_NO_FILE      12
#define EXCP_INT          13
#define EXCP_NUM_FORMAT   14
#define EXCP_STR_IDXZ_OOB 15


int hb_excp_str_to_type (char * str);
void hb_throw_and_create_excp (u1 type);
void hb_throw_exception (struct obj_ref * eref);
