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
#include <stdlib.h>
#include <string.h>

#include <types.h>
#include <thread.h>
#include <stack.h>
#include <class.h>

jthread_t *
hb_create_thread (java_class_t * cls, const char * name)
{
	jthread_t * t = malloc(sizeof(jthread_t));
	if (!t) {
		HB_ERR("Could not create thread for class (%s)\n", hb_get_class_name(cls));
		return NULL;
	}
	memset(t, 0, sizeof(jthread_t));

	THR_DEBUG("Creating thread for class (%s)\n", hb_get_class_name(cls));

	t->class     = cls;
	t->cur_frame = NULL;
	t->name      = name;

	return t;
}
