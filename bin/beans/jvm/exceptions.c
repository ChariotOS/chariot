/*
 * This file is part of the Hawkbeans JVM developed by
 * the HExSA Lab at Illinois Institute of Technology.
 *
 * Copyright (c) 2019, Kyle C. Hale <khale@cs.iit.edu>
 *
 * All rights reserved.
 *
 * Author: Kyle C. Hale <khale@cs.iit.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the
 * file "LICENSE.txt".
 */
#include <bc_interp.h>
#include <class.h>
#include <exceptions.h>
#include <gc.h>
#include <mm.h>
#include <stack.h>
#include <stdlib.h>
#include <string.h>
#include <thread.h>
#include <types.h>

extern jthread_t* cur_thread;

/*
 * Maps internal exception identifiers to fully
 * qualified class paths for the exception classes.
 * Note that the ones without fully qualified paths
 * will not be properly raised.
 *
 * TODO: add the classes for these
 *
 */
#define EXCP_STR_LEN (16)

static const char* excp_strs[EXCP_STR_LEN] __attribute__((used)) = {
    "java/lang/NullPointerException",
    "java/lang/IndexOutOfBoundsException",
    "java/lang/ArrayIndexOutOfBoundsException",
    "IncompatibleClassChangeError",
    "java/lang/NegativeArraySizeException",
    "java/lang/OutOfMemoryError",
    "java/lang/ClassNotFoundException",
    "java/lang/ArithmeticException",
    "java/lang/NoSuchFieldError",
    "java/lang/NoSuchMethodError",
    "java/lang/RuntimeException",
    "java/io/IOException",
    "FileNotFoundException",
    "java/lang/InterruptedException",
    "java/lang/NumberFormatException",
    "java/lang/StringIndexOutOfBoundsException",
};

#define HB_EXCEPTION_THROWABLE_NAME "java/lang/Throwable"

int hb_excp_str_to_type(char* str) {
  for (int i = 0; i < sizeof(excp_strs) / sizeof(char*); i++) {
    if (strstr(excp_strs[i], str)) return i;
  }
  return -1;
}

/*
 * Throws an exception given an internal ID
 * that refers to an exception type. This is to
 * be used by the runtime (there is no existing
 * exception object, so we have to create a new one
 * and init it).
 *
 * @return: none. exits on failure.
 *
 */
// WRITE ME
void hb_throw_and_create_excp(u1 type) {
  if (type > EXCP_STR_LEN) {
    HB_ERR("%s: bad builtin exception idx (%d)", __func__, type);
    exit(EXIT_FAILURE);
  }

  const char *class_name = excp_strs[type];
  java_class_t *cls = hb_get_or_load_class(class_name);
  if (!cls) {
    HB_ERR("%s: unable to load builtin exception class '%s'", __func__, class_name);
    exit(EXIT_FAILURE);
  }

  obj_ref_t *oa = gc_obj_alloc(cls);
  if (!oa) {
    HB_ERR("%s: unable to throw exception, out of memory", __func__);
    exit(EXIT_FAILURE);
  }

  native_obj_t *aobj = (native_obj_t*) oa->heap_ptr;
  aobj->class = cls;

  hb_throw_exception(oa);
}

/*
 * gets the exception message from the object
 * ref referring to the exception object.
 *
 * NOTE: caller must free the string
 *
 */
static char* get_excp_str(obj_ref_t* eref) {
  char* ret;
  native_obj_t* obj = (native_obj_t*)eref->heap_ptr;

  obj_ref_t* str_ref = obj->fields[0].obj;
  native_obj_t* str_obj;
  obj_ref_t* arr_ref;
  native_obj_t* arr_obj;
  int i;

  if (!str_ref) {
    return NULL;
  }

  str_obj = (native_obj_t*)str_ref->heap_ptr;

  arr_ref = str_obj->fields[0].obj;

  if (!arr_ref) {
    return NULL;
  }

  arr_obj = (native_obj_t*)arr_ref->heap_ptr;

  ret = malloc(arr_obj->flags.array.length + 1);

  for (i = 0; i < arr_obj->flags.array.length; i++) {
    ret[i] = arr_obj->fields[i].char_val;
  }

  ret[i] = 0;

  return ret;
}

/*
 * Throws an exception using an
 * object reference to some exception object (which
 * implements Throwable). To be used with athrow.
 * If we're given a bad reference, we throw a
 * NullPointerException.
 *
 * @return: none. exits on failure.
 *
 */
void hb_throw_exception(obj_ref_t* eref)
{
  native_obj_t *aobj = (native_obj_t*) eref->heap_ptr;
  if (!aobj) {
    HB_ERR("%s: referenced object is null", __func__);
    exit(EXIT_FAILURE);
  }

  java_class_t *thrown_cls = aobj->class;
  if (!thrown_cls) {
    HB_ERR("%s: no class is associated with object", __func__);
    exit(EXIT_FAILURE);
  }

  // ensure that the object extends Throwable
  const char *throw_super_class_name;
  java_class_t *throw_super_cls = thrown_cls;
  while (true) {
    if (!throw_super_cls) {
      HB_ERR("%s: attempted to throw an object that does not inherit Throwable", __func__);
      exit(EXIT_FAILURE);
    }

    throw_super_class_name = hb_get_class_name(throw_super_cls);
    if (strcmp(throw_super_class_name, HB_EXCEPTION_THROWABLE_NAME) == 0) {
      break;
    }

    throw_super_cls = hb_get_super_class(throw_super_cls);
  }

  stack_frame_t *cur_frame = cur_thread->cur_frame;
  while (true) {
    int pc = cur_frame->pc;

    code_attr_t *attr = cur_frame->minfo->code_attr;
    excp_table_t *excp_entries = attr->excp_table;
    const int excp_entries_len = attr->excp_table_len;

    excp_table_t *cur_entry;
    excp_table_t *excp_entry_end = excp_entries + excp_entries_len;
    for (cur_entry = excp_entries; cur_entry < excp_entry_end; cur_entry++) {
      if (!(pc >= cur_entry->start_pc && pc <= cur_entry->end_pc)) {
        continue;
      }

      /* resolve the class of the catch type handler */
      java_class_t *catch_class;
      {
        u2 handler_class_idx = cur_entry->catch_type;

        /* if the `catch_type` == 0, then we have reached the finally */
        if (handler_class_idx == 0) {
          goto run_handler;
        }

        java_class_t *cur_class = cur_frame->cls;

        const_pool_info_t **pool_slot =
            &cur_class->const_pool[handler_class_idx];

        CONSTANT_Class_info_t *class_ref =
            (CONSTANT_Class_info_t *) *pool_slot;
        if (IS_RESOLVED(class_ref)) {
          catch_class = (java_class_t *) MASK_RESOLVED_BIT(class_ref);
        }
        // TODO NULL CHECK
        if (class_ref->tag != CONSTANT_Class) {
          HB_ERR("%s: tag is not of Class!", __func__);
          exit(EXIT_FAILURE);
        }

        catch_class = hb_resolve_class(handler_class_idx, cur_class);
        if (!catch_class) {
          HB_ERR("%s: unable to exception handler class", __func__);
          exit(EXIT_FAILURE);
        }

        *pool_slot = (const_pool_info_t *) MARK_RESOLVED(cur_class);
      }

      /*
       * now that we have resolved the catch class,
       * check if the thrown class is a subclass of `catch_class`
       */
      java_class_t *cur_thrown_super_cls = thrown_cls;
      while (true) {
        if (!cur_thrown_super_cls) {
          /* reached the end of the super classes, not a match */
          goto skip_entry;
        }

        if (cur_thrown_super_cls == catch_class) {
          /* we've reached a super class of the thrown exception that is being
           * handled by this catch entry */
          break;
        }

        cur_thrown_super_cls = hb_get_super_class(cur_thrown_super_cls);
      }

    run_handler:
      {
        /*
         * set exception to the local and the pc to the handler's body
         */
        var_t v = {
            .tag = TAG_OBJ,
            .obj = eref,
        };

        /*
         * push exception on to the stack to rethrow
         */

        op_stack_t *stack = cur_frame->op_stack;

        /* Citing JVM Spec Chapter 6 with `athrow`:
         *
         *    If a handler for this exception is matched in the current method,
         *    the athrow instruction discards all the values on the operand
         *    stack, then pushes the thrown object onto the operand stack.
         */
        stack->sp = 0;
        stack->oprs[++(stack->sp)] = v;

        cur_frame->pc = cur_entry->handler_pc;
        return;
      }

    skip_entry:
      continue;
    }

    /*
     * unable to find a catch entry that matches the thrown exception
     * pop current stack frame and try again
     */


    hb_pop_frame(cur_thread);
    cur_frame = cur_thread->cur_frame;
    if (!cur_frame) {
      char *excp_display = get_excp_str(eref);
      if (!excp_display) {
        excp_display = "";
      }
      const char *class_name = hb_get_class_name(thrown_cls);
      HB_ERR("[jvm] unhandled exception in thread \"main\": %s: %s",
             class_name, excp_display);
      exit(EXIT_FAILURE);
    }

    /*
     * otherwise, we still have more catch entries to check
     */
  }
}
