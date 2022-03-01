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
#include <types.h>
#include <list.h>

#if DEBUG_THREAD == 1
#define THR_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define THR_DEBUG(fmt, args...)
#endif


typedef struct jthread {

	struct stack_frame * cur_frame;

	struct java_class * class;

	struct gc_state * gc_state;

	const char * name;
	
} jthread_t;


jthread_t * 
hb_create_thread(struct java_class * cls, const char * name);
