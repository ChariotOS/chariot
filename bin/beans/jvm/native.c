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
#include <stdio.h>
#include <unistd.h>

#include <native.h>
#include <types.h>
#include <thread.h>
#include <class.h>
#include <stack.h>
#include <bc_interp.h>

extern jthread_t * cur_thread;

static int
handle_putCharToStdout0 (obj_ref_t * thisref, int b)
{
	putchar(b);
	return 0;
}

static int
handle_putStringToStdout0 (obj_ref_t * thisref, obj_ref_t * strref)
{
	obj_ref_t * arr_ref;
	native_obj_t * strobj;
	native_obj_t * arr_obj;
	int i;

	strobj = (native_obj_t*)strref->heap_ptr;

	arr_ref = strobj->fields[0].obj;
	arr_obj = (native_obj_t*)arr_ref->heap_ptr;

	for (i = 0; i < arr_obj->flags.array.length; i++) {
		 putchar(arr_obj->fields[i].char_val);
	}

	return 0;
}


static int
handle_sleep (long msec)
{
	usleep(msec * 1000);
	return 0;
}

static int
handle_exit (int code)
{
	exit(code);
	return 0;
}


int
hb_handle_native (method_info_t * mi,
                  java_class_t * cls,
                  u1 invoke_type)
{
	int parm_count;
	const char * mname = hb_get_const_str(mi->name_idx, mi->owner);
	op_stack_t * op_stack = cur_thread->cur_frame->op_stack;
	int nwide;

	parm_count = hb_get_parm_count_from_method(cur_thread, mi, invoke_type, &nwide);

	NATIVE_DEBUG("Handling native method (%s), %d args\n",
		hb_get_const_str(mi->name_idx, mi->owner), parm_count);

	if (strcmp(mname, "putStringToStdout0") == 0) {
		var_t str = op_stack->oprs[op_stack->sp--];
		var_t this = op_stack->oprs[op_stack->sp--];
		if (handle_putStringToStdout0(this.obj, str.obj) != 0) {
			HB_ERR("Could not handle putStringToStdout0\n");
			return -1;
		}
	} else if (strcmp(mname, "putCharToStdout0") == 0) {
		var_t b = op_stack->oprs[op_stack->sp--];
		var_t this = op_stack->oprs[op_stack->sp--];
		if (handle_putCharToStdout0(this.obj, b.int_val) != 0) {
			HB_ERR("Could not handle putCharToStdout0\n");
			return -1;
		}
	} else if (strcmp(mname, "sleep") == 0) {
		var_t msec = op_stack->oprs[op_stack->sp--];
		if (handle_sleep(msec.long_val) != 0) {
			HB_ERR("Could not handle sleep\n");
			return -1;
		}
	} else if (strcmp(mname, "exit") == 0) {
		var_t code = op_stack->oprs[op_stack->sp--];
		if (handle_exit((int)code.int_val) != 0) {
			HB_ERR("Could not handle exit\n");
			return -1;
		}
	} else {
		HB_ERR("Unhandled native method %s\n", mname);
		return -1;
	}

	return 0;
}

