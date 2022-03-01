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

#include <hawkbeans.h>
#include <thread.h>
#include <class.h>

#if DEBUG_INTERP == 1
#define BC_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define BC_DEBUG(fmt, args...)
#endif

#define ESHOULD_RETURN 2
#define ESHOULD_BRANCH 3
#define ETHREAD_DEATH  4

int hb_invoke_ctor(struct obj_ref * oref);
int hb_instr_repr(jthread_t * t, char * buf, size_t buflen);
int hb_exec_one(jthread_t * t);
int hb_exec_method(jthread_t * t);
