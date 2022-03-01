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

#if DEBUG_NATIVE == 1
#define NATIVE_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define NATIVE_DEBUG(fmt, args...)
#endif

struct method_info;
struct java_class;

int hb_handle_native (struct method_info * mi, 
		      struct java_class * cls,
		      u1 invoke_type);
