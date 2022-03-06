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
#define _CHARIOT_SRC
#include <stdlib.h>

#include <class.h>
#include <thread.h>
#include <types.h>
#include <stack.h>
#include <methods.h>
#include <constants.h>
#include <mm.h>
#include <bc_interp.h>
#include <native.h>
#include <exceptions.h>
#include <gc.h>

#include <mnemonics.h>

#include <arch/x64-linux/bootstrap_loader.h>

#define GET_2B_IDX(bcp) ((u2)((bcp)[1] << 8u) | (u2)(bcp)[2])

#define GET_4B_IDX(bcp) ((u4)(bcp)[1] << 24u | (u4)(bcp)[2] << 16u | (u4)(bcp)[3] << 8u | (u4)(bcp)[0])

extern jthread_t *cur_thread;

typedef int (*op_handler_t)(u1 *bc, java_class_t *cls);

/* KCH NOTE: THIS IMPLEMENTATION'S STACK GROWS *UP* */
static inline void push_val(var_t v, u1 tag) {
  if (!(tag >= TAG_MIN && tag <= TAG_MAX)) {
    HB_ERR("%s: bad tag provided, got %d = '%s'", __func__, tag, TAG_STR(tag));
  }

  v.tag = tag;
  op_stack_t *stack = cur_thread->cur_frame->op_stack;

  if (stack->max_oprs < stack->sp + 1) HB_WARN("Stack overflow!");

	printf("push %02x %p\n", v.tag, v.obj);
  stack->oprs[++(stack->sp)] = v;
}


static inline var_t pop_val(u1 expected_tag_flags) {
  op_stack_t *stack = cur_thread->cur_frame->op_stack;
  var_t v = stack->oprs[stack->sp];
  stack->sp--;

  if (stack->sp < 0) HB_WARN("Stack underflow!");

  if ((v.tag & expected_tag_flags) == 0 && expected_tag_flags != TAG_ANY) {
    HB_ERR("%s: got wrong variable tag, expected '%s', got '%s'", __func__, TAG_STR(expected_tag_flags), TAG_STR(v.tag));
  }

	printf("pop %02x %p\n", v.tag, v.obj);
  return v;
}

static int get_const(u2 idx, java_class_t *cls, var_t *ret) {
  u1 tag = cls->const_pool[idx]->tag;
  switch (tag) {
    case CONSTANT_Integer: {
      CONSTANT_Integer_info_t *ii = (CONSTANT_Integer_info_t *)cls->const_pool[idx];
      ret->int_val = ii->bytes;
      return TAG_INT;
    }
    case CONSTANT_Float: {
      CONSTANT_Float_info_t *fi = (CONSTANT_Float_info_t *)cls->const_pool[idx];
      ret->float_val = fi->bytes;
      return TAG_FLOAT;
    }
    case CONSTANT_String: {
      CONSTANT_String_info_t *si = (CONSTANT_String_info_t *)cls->const_pool[idx];
      obj_ref_t *ref = gc_str_obj_alloc(hb_get_const_str(si->str_idx, cls));
      if (!ref) {
        hb_throw_and_create_excp(EXCP_OOM);
        return -ESHOULD_BRANCH;
      }
      ret->obj = ref;
      return TAG_OBJ;
    }
    default:
      HB_ERR("Unsupported const type (%d)", cls->const_pool[idx]->tag);
      return TAG_BAD;
  }
}

static int get_const_wide(u2 idx, java_class_t *cls, var_t *ret) {
  switch (cls->const_pool[idx]->tag) {
    case CONSTANT_Long: {
      CONSTANT_Long_info_t *li = (CONSTANT_Long_info_t *)cls->const_pool[idx];
      ret->long_val = (u8)li->lo_bytes | ((u8)li->hi_bytes << 32u);
      return TAG_LONG;
    }
    case CONSTANT_Double: {
      CONSTANT_Double_info_t *di = (CONSTANT_Double_info_t *)cls->const_pool[idx];
      ret->dbl_val = (d8)((u8)di->lo_bytes | ((u8)di->hi_bytes << 32u));
      return TAG_DOUBLE;
    }
    default:
      HB_ERR("Unsupported const type (%d) in %s", cls->const_pool[idx]->tag, __func__);
      return TAG_BAD;
  }
}



static int handle_invalid(u1 *bc, java_class_t *cls) {
  HB_ERR("ENCOUNTERED INVALID INSTRUCTION");
  return -1;
}

static int handle_nop(u1 *bc, java_class_t *cls) { return 1; }

static int handle_aconst_null(u1 *bc, java_class_t *cls) {
  var_t v;
  v.obj = NULL;
  push_val(v, TAG_OBJ);
  return 1;
}

#define DO_ICONSTN(n)   \
  var_t v;              \
  v.int_val = n;        \
  push_val(v, TAG_INT); \
  return 1;

static int handle_iconst_m1(u1 *bc, java_class_t *cls) { DO_ICONSTN(-1); }

static int handle_iconst_0(u1 *bc, java_class_t *cls) { DO_ICONSTN(0); }

static int handle_iconst_1(u1 *bc, java_class_t *cls) { DO_ICONSTN(1); }

static int handle_iconst_2(u1 *bc, java_class_t *cls) { DO_ICONSTN(2); }

static int handle_iconst_3(u1 *bc, java_class_t *cls) { DO_ICONSTN(3); }

static int handle_iconst_4(u1 *bc, java_class_t *cls) { DO_ICONSTN(4); }

static int handle_iconst_5(u1 *bc, java_class_t *cls) { DO_ICONSTN(5); }

static int handle_lconst_0(u1 *bc, java_class_t *cls) {
  var_t v;
  v.long_val = 0;
  push_val(v, TAG_LONG);
  return 1;
}

static int handle_lconst_1(u1 *bc, java_class_t *cls) {
  var_t v;
  v.long_val = 1;
  push_val(v, TAG_LONG);
  return 1;
}

#define DO_FCONSTN(n)     \
  var_t v;                \
  v.float_val = n;        \
  push_val(v, TAG_FLOAT); \
  return 1;

static int handle_fconst_0(u1 *bc, java_class_t *cls) { DO_FCONSTN(0.0); }

static int handle_fconst_1(u1 *bc, java_class_t *cls) { DO_FCONSTN(1.0); }

static int handle_fconst_2(u1 *bc, java_class_t *cls) { DO_FCONSTN(2.0); }

static int handle_dconst_0(u1 *bc, java_class_t *cls) {
  var_t v;
  v.dbl_val = 0.0;
  push_val(v, TAG_DOUBLE);
  return 1;
}

static int handle_dconst_1(u1 *bc, java_class_t *cls) {
  var_t v;
  v.dbl_val = 1.0;
  push_val(v, TAG_DOUBLE);
  return 1;
}

static int handle_bipush(u1 *bc, java_class_t *cls) {
  var_t v;
  v.int_val = (int)(char)bc[1];
  push_val(v, TAG_INT);
  return 2;
}

static int handle_sipush(u1 *bc, java_class_t *cls) {
  var_t v;
  v.int_val = (int)(short)GET_2B_IDX(bc);
  push_val(v, TAG_INT);
  return 3;
}

static int handle_ldc(u1 *bc, java_class_t *cls) {
  var_t v;
  int ret;
  ret = get_const((u2)bc[1], cls, &v);
  if (ret <= 0) {
    return ret;
  }
  push_val(v, ret);
  return 2;
}

static int handle_ldc_w(u1 *bc, java_class_t *cls) {
  u2 idx = GET_2B_IDX(bc);
  var_t v;
  int ret;
  ret = get_const(idx, cls, &v);
  if (ret <= 0) {
    return ret;
  }
  push_val(v, ret);
  return 3;
}

static int handle_ldc2_w(u1 *bc, java_class_t *cls) {
  u2 idx = GET_2B_IDX(bc);
  var_t v;
  u1 tag = get_const_wide(idx, cls, &v);
  if (tag == TAG_BAD) {
    HB_ERR("Could not get const");
    return -1;
  }
  push_val(v, tag);
  return 3;
}

#define DO_ILOADN(IDX)                          \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = frame->locals[IDX];                 \
  push_val(v, TAG_INT);                         \
  return 1;

static int handle_iload(u1 *bc, java_class_t *cls) {
  u1 idx = bc[1];
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = frame->locals[idx];
  push_val(v, TAG_INT);
  return 2;
}

static int handle_lload(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  u1 idx = bc[1];
  var_t v;
  v.long_val = ((u8)frame->locals[idx].int_val) | (((u8)frame->locals[idx + 1].int_val) << 32u);
  push_val(v, TAG_LONG);
  return 2;
}

static int handle_fload(u1 *bc, java_class_t *cls) {
  u1 idx = bc[1];
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = frame->locals[idx];
  EXPECT_TAG(v, TAG_FLOAT);
  push_val(v, TAG_FLOAT);
  return 2;
}

static int handle_dload(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  u1 idx = bc[1];
  var_t v;
  v.long_val = (u8)frame->locals[idx].int_val | (((u8)frame->locals[idx + 1].int_val) << 32u);
  push_val(v, TAG_DOUBLE);
  return 2;
}

static int handle_aload(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  u1 idx = bc[1];
  var_t v = frame->locals[idx];
  // EXPECT_TAG(v, TAG_OBJ);
  push_val(v, TAG_OBJ);
  return 2;
}

static int handle_iload_0(u1 *bc, java_class_t *cls) { DO_ILOADN(0) }

static int handle_iload_1(u1 *bc, java_class_t *cls) { DO_ILOADN(1) }

static int handle_iload_2(u1 *bc, java_class_t *cls) { DO_ILOADN(2) }

static int handle_iload_3(u1 *bc, java_class_t *cls) { DO_ILOADN(3) }

#define DO_LLOADN(n)                             \
  stack_frame_t *frame = cur_thread->cur_frame;  \
  u8 a, b;                                       \
  var_t res;                                     \
  a = (u8)frame->locals[n].long_val;             \
  b = (u8)frame->locals[n + 1].long_val;         \
  res.long_val = (a & 0xFFFFFFFFu) | (b << 32u); \
  push_val(res, TAG_LONG);                       \
  return 1;

static int handle_lload_0(u1 *bc, java_class_t *cls) { DO_LLOADN(0); }

static int handle_lload_1(u1 *bc, java_class_t *cls) { DO_LLOADN(1); }

static int handle_lload_2(u1 *bc, java_class_t *cls) { DO_LLOADN(2); }

static int handle_lload_3(u1 *bc, java_class_t *cls) { DO_LLOADN(3); }

#define DO_FLOADN(n)                            \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = frame->locals[n];                   \
  EXPECT_TAG(v, TAG_FLOAT);                     \
  push_val(v, TAG_FLOAT);                       \
  return 1;

static int handle_fload_0(u1 *bc, java_class_t *cls) { DO_FLOADN(0); }

static int handle_fload_1(u1 *bc, java_class_t *cls) { DO_FLOADN(1); }

static int handle_fload_2(u1 *bc, java_class_t *cls) { DO_FLOADN(2); }

static int handle_fload_3(u1 *bc, java_class_t *cls) { DO_FLOADN(3); }

static int handle_dload_0(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dload_1(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dload_2(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dload_3(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

#define DO_ALOADN(n)                            \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = frame->locals[n];                   \
  push_val(v, TAG_OBJ);                         \
  return 1;

static int handle_aload_0(u1 *bc, java_class_t *cls) { DO_ALOADN(0); }

static int handle_aload_1(u1 *bc, java_class_t *cls) { DO_ALOADN(1); }

static int handle_aload_2(u1 *bc, java_class_t *cls) { DO_ALOADN(2); }

static int handle_aload_3(u1 *bc, java_class_t *cls) { DO_ALOADN(3); }

static int handle_iaload(u1 *bc, java_class_t *cls) {
  var_t idx = pop_val(TAG_INT);
  var_t a = pop_val(TAG_OBJ);
  var_t res;
  obj_ref_t *ref = a.obj;
  native_obj_t *arr;

  if (!ref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr = (native_obj_t *)ref->heap_ptr;

  if (idx.int_val > arr->flags.array.length - 1) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  res.int_val = arr->fields[idx.int_val].int_val;

  push_val(res, TAG_INT);

  return 1;
}

static int handle_laload(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_faload(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_daload(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_aaload(u1 *bc, java_class_t *cls) {
  var_t idx = pop_val(TAG_INT);
  var_t arr = pop_val(TAG_OBJ);
  obj_ref_t *arr_ref = arr.obj;
  native_obj_t *arr_obj = NULL;

  if (!arr_ref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr_obj = (native_obj_t *)arr_ref->heap_ptr;

  if (idx.int_val > arr_obj->flags.array.length - 1) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  var_t v = arr_obj->fields[idx.int_val];
  EXPECT_TAG(v, TAG_OBJ);
  push_val(v, TAG_OBJ);

  return 1;
}

static int handle_baload(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_caload(u1 *bc, java_class_t *cls) {
  var_t idx = pop_val(TAG_INT);
  var_t aref = pop_val(TAG_OBJ);
  var_t res;
  native_obj_t *arr;

  if (!aref.obj) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr = (native_obj_t *)aref.obj->heap_ptr;

  if (idx.int_val > arr->flags.array.length - 1) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  res.int_val = (int)arr->fields[idx.int_val].char_val;

  push_val(res, TAG_INT);

  return 1;
}

static int handle_saload(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}


static int handle_istore(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = pop_val(TAG_INT_LIKE);
  u1 idx = bc[1];
  frame->locals[idx] = v;
  return 2;
}

static int handle_lstore(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = pop_val(TAG_LONG);
  u1 idx = bc[1];
  frame->locals[idx].int_val = v.long_val & 0xFFFFFFFF;
  frame->locals[idx + 1].int_val = v.long_val >> 32u;
  return 2;
}

static int handle_fstore(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = pop_val(TAG_FLOAT);
  u1 idx = bc[1];
  frame->locals[idx].float_val = v.float_val;
  return 2;
}

static int handle_dstore(u1 *bc, java_class_t *cls) {
  stack_frame_t *frame = cur_thread->cur_frame;
  var_t v = pop_val(TAG_DOUBLE);
  u1 idx = bc[1];
  frame->locals[idx].int_val = v.long_val & 0xFFFFFFFF;
  frame->locals[idx + 1].int_val = v.long_val >> 32;
  return 2;
}

static int handle_astore(u1 *bc, java_class_t *cls) {
  var_t ov = pop_val(TAG_OBJ);
  u1 idx = bc[1];

  cur_thread->cur_frame->locals[idx] = ov;

  return 2;
}

#define DO_ISTOREN(n)                           \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = pop_val(TAG_INT_LIKE);              \
  frame->locals[n] = v;                         \
  return 1;

static int handle_istore_0(u1 *bc, java_class_t *cls) { DO_ISTOREN(0) }

static int handle_istore_1(u1 *bc, java_class_t *cls) { DO_ISTOREN(1) }

static int handle_istore_2(u1 *bc, java_class_t *cls) { DO_ISTOREN(2) }

static int handle_istore_3(u1 *bc, java_class_t *cls) { DO_ISTOREN(3) }

#define DO_LSTOREN(n)                                  \
  stack_frame_t *frame = cur_thread->cur_frame;        \
  var_t v = pop_val(TAG_LONG);                         \
  frame->locals[n].int_val = v.long_val & 0xFFFFFFFFu; \
  frame->locals[n + 1].int_val = v.long_val >> 32u;    \
  return 1;

static int handle_lstore_0(u1 *bc, java_class_t *cls) { DO_LSTOREN(0); }

static int handle_lstore_1(u1 *bc, java_class_t *cls) { DO_LSTOREN(1); }

static int handle_lstore_2(u1 *bc, java_class_t *cls) { DO_LSTOREN(2); }

static int handle_lstore_3(u1 *bc, java_class_t *cls) { DO_LSTOREN(3); }

#define DO_FSTOREN(n)                           \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = pop_val(TAG_FLOAT);                 \
  frame->locals[n].float_val = v.float_val;     \
  return 1;

static int handle_fstore_0(u1 *bc, java_class_t *cls) { DO_FSTOREN(0); }

static int handle_fstore_1(u1 *bc, java_class_t *cls) { DO_FSTOREN(1); }

static int handle_fstore_2(u1 *bc, java_class_t *cls) { DO_FSTOREN(2); }

static int handle_fstore_3(u1 *bc, java_class_t *cls) { DO_FSTOREN(3); }

#define DO_DSTOREN(n)                                  \
  stack_frame_t *frame = cur_thread->cur_frame;        \
  var_t v = pop_val(TAG_DOUBLE);                       \
  frame->locals[n].int_val = v.long_val & 0xFFFFFFFFu; \
  frame->locals[n + 1].int_val = v.long_val >> 32u;    \
  return 1;

static int handle_dstore_0(u1 *bc, java_class_t *cls) { DO_DSTOREN(0); }

static int handle_dstore_1(u1 *bc, java_class_t *cls) { DO_DSTOREN(1); }

static int handle_dstore_2(u1 *bc, java_class_t *cls) { DO_DSTOREN(2); }

static int handle_dstore_3(u1 *bc, java_class_t *cls) { DO_DSTOREN(3); }

#define DO_ASTOREN(n)                           \
  stack_frame_t *frame = cur_thread->cur_frame; \
  var_t v = pop_val(TAG_OBJ);                   \
  frame->locals[n].obj = v.obj;                 \
  return 1;

static int handle_astore_0(u1 *bc, java_class_t *cls) { DO_ASTOREN(0); }

static int handle_astore_1(u1 *bc, java_class_t *cls) { DO_ASTOREN(1); }

static int handle_astore_2(u1 *bc, java_class_t *cls) { DO_ASTOREN(2); }

static int handle_astore_3(u1 *bc, java_class_t *cls) { DO_ASTOREN(3); }

static int handle_iastore(u1 *bc, java_class_t *cls) {
  var_t v = pop_val(TAG_INT);
  var_t idx = pop_val(TAG_INT);
  var_t a = pop_val(TAG_OBJ);
  obj_ref_t *ref = a.obj;
  native_obj_t *arr;

  if (!ref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr = (native_obj_t *)ref->heap_ptr;

  if (idx.int_val > arr->flags.array.length - 1) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  arr->fields[idx.int_val] = v;
  return 1;
}

static int handle_lastore(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_fastore(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dastore(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_aastore(u1 *bc, java_class_t *cls) {
  var_t val = pop_val(TAG_OBJ);
  var_t idx = pop_val(TAG_INT);
  var_t arr = pop_val(TAG_OBJ);
  obj_ref_t *arr_ref = arr.obj;
  native_obj_t *arr_obj = NULL;

  if (!arr_ref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr_obj = (native_obj_t *)arr_ref->heap_ptr;

  if (idx.int_val > arr_obj->flags.array.length - 1) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  // TODO: type checking
  arr_obj->fields[idx.int_val] = val;

  return 1;
}

static int handle_bastore(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_castore(u1 *bc, java_class_t *cls) {
  var_t v = pop_val(TAG_CHAR);
  var_t idx = pop_val(TAG_INT);
  var_t a = pop_val(TAG_OBJ);
  obj_ref_t *ref = a.obj;
  native_obj_t *arr;
  char x;

  if (!ref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  arr = (native_obj_t *)ref->heap_ptr;
  if (!(arr && arr->order > 0)) {
    HB_ERR("%s: invalid ref at %p has invalid heap ptr %p", __func__, ref, arr);
    exit(EXIT_FAILURE);
  }

  if (idx.int_val >= arr->flags.array.length) {
    hb_throw_and_create_excp(EXCP_ARR_IDX_OOB);
    return -ESHOULD_BRANCH;
  }

  x = (char)v.int_val;

  arr->fields[idx.int_val].char_val = x;

  return 1;
}

static int handle_sastore(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_pop(u1 *bc, java_class_t *cls) {
  pop_val(TAG_ANY);
  return 1;
}

static int handle_pop2(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dup(u1 *bc, java_class_t *cls) {
  op_stack_t *stack = cur_thread->cur_frame->op_stack;
  var_t v = stack->oprs[stack->sp];
  stack->oprs[++(stack->sp)] = v;
  return 1;
}

static int handle_dup_x1(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dup_x2(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

/* KCH TODO: this does not handle computational type 2 (long,double)*/
static int handle_dup2(u1 *bc, java_class_t *cls) {
  op_stack_t *stack = cur_thread->cur_frame->op_stack;
  push_val(stack->oprs[stack->sp - 1], TAG_ANY);
  // note still -1 since push_val modifies sp
  push_val(stack->oprs[stack->sp - 1], TAG_ANY);
  return 1;
}

static int handle_dup2_x1(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dup2_x2(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_swap(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_iadd(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  a = pop_val(TAG_INT);
  b = pop_val(TAG_INT);
  c.int_val = (u4)((i4)a.int_val + (i4)b.int_val);
  push_val(c, TAG_INT);
  return 1;
}

static int handle_ladd(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  a = pop_val(TAG_LONG);
  b = pop_val(TAG_LONG);
  c.long_val = (u8)((i8)a.long_val + (i8)b.long_val);
  push_val(c, TAG_LONG);
  return 1;
}

static int handle_fadd(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  a = pop_val(TAG_FLOAT);
  b = pop_val(TAG_FLOAT);
  c.float_val = a.float_val + b.float_val;
  push_val(c, TAG_FLOAT);
  return 1;
}

static int handle_dadd(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  a = pop_val(TAG_DOUBLE);
  b = pop_val(TAG_DOUBLE);
  c.dbl_val = a.dbl_val + b.dbl_val;
  push_val(c, TAG_DOUBLE);
  return 1;
}

static int handle_isub(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  b = pop_val(TAG_INT);
  a = pop_val(TAG_INT);
  c.long_val = (u8)((int32_t)a.int_val - (int32_t)b.int_val);
  push_val(c, TAG_INT);
  return 1;
}

static int handle_lsub(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  b = pop_val(TAG_LONG);
  a = pop_val(TAG_LONG);
  c.long_val = (u8)((i8)a.long_val - (i8)b.long_val);
  push_val(c, TAG_LONG);
  return 1;
}

static int handle_fsub(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  b = pop_val(TAG_FLOAT);
  a = pop_val(TAG_FLOAT);
  c.float_val = a.float_val - b.float_val;
  push_val(c, TAG_FLOAT);
  return 1;
}

static int handle_dsub(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  b = pop_val(TAG_DOUBLE);
  a = pop_val(TAG_DOUBLE);
  c.dbl_val = a.dbl_val - b.dbl_val;
  push_val(c, TAG_DOUBLE);
  return 1;
}

static int handle_imul(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  i4 m, v1, v2;
  b = pop_val(TAG_INT);
  a = pop_val(TAG_INT);

  v1 = (i4)a.int_val;
  v2 = (i4)b.int_val;

  m = v1 * v2;

  c.int_val = (u4)m;

  push_val(c, TAG_INT);
  return 1;
}

static int handle_lmul(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  long m, v1, v2;
  b = pop_val(TAG_LONG);
  a = pop_val(TAG_LONG);

  v1 = (i8)a.long_val;
  v2 = (i8)b.long_val;

  m = v1 * v2;

  c.long_val = (u8)m;

  push_val(c, TAG_LONG);
  return 1;
}

static int handle_fmul(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dmul(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_idiv(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  i4 m, v1, v2;
  b = pop_val(TAG_INT);
  a = pop_val(TAG_INT);

  v1 = (i4)a.int_val;
  v2 = (i4)b.int_val;

  if (v2 == 0) {
    /* act as if this instruction never happened */
    push_val(a, TAG_INT);
    push_val(b, TAG_INT);

    // Throw ArithmeticException() if divisor is zero"
    hb_throw_and_create_excp(EXCP_ARITH);
    return -ESHOULD_BRANCH;
  } else {
    m = v1 / v2;
  }

  c.int_val = (u4)m;
  push_val(c, TAG_INT);
  return 1;
}

static int handle_ldiv(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_fdiv(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ddiv(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

// WRITE ME: be careful with exceptions
static int handle_irem(u1 *bc, java_class_t *cls) {
  var_t a, b, c;
  i4 m, v1, v2;
  b = pop_val(TAG_INT);
  a = pop_val(TAG_INT);

  v1 = (i4)a.int_val;
  v2 = (i4)b.int_val;

  if (v2 == 0) {
    /* act as if this instruction never happened */
    push_val(a, TAG_INT);
    push_val(b, TAG_INT);

    // Throw ArithmeticException() if divisor is zero"
    hb_throw_and_create_excp(EXCP_ARITH);
    return -ESHOULD_BRANCH;
  } else {
    m = v1 % v2;
  }

  c.int_val = (u4)m;
  push_val(c, TAG_INT);
  return 1;
}

static int handle_lrem(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_frem(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_drem(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ineg(u1 *bc, java_class_t *cls) {
  var_t v = pop_val(TAG_INT);
  var_t res;
  i4 n = (i4)v.int_val;
  res.int_val = (u4)(-n);
  push_val(res, TAG_INT);
  return 1;
}

static int handle_lneg(u1 *bc, java_class_t *cls) {
  var_t v = pop_val(TAG_LONG);
  var_t res;
  i8 n = (i8)v.long_val;
  res.long_val = (u8)(-n);
  push_val(res, TAG_LONG);
  return 1;
}

static int handle_fneg(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dneg(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ishl(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_lshl(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ishr(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_lshr(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_iushr(u1 *bc, java_class_t *cls) {
  i4 a, b;
  var_t v1, v2, res;
  v2 = pop_val(TAG_INT);
  v1 = pop_val(TAG_INT);
  a = (i4)v1.int_val;
  // shift right by low 5 bits of second val
  b = ((u4)v2.int_val) & 0x1fu;
  // The ugly casts are because C will use logical shift for unsigned,
  // but arithmetic shift for signed, although this is impl-dependent
  res.int_val = ((a >= 0) ? (u4)((u4)a >> b) : (u4)(((u4)a >> b) + (2u << ~b)));
  push_val(res, TAG_INT);
  return 1;
}

static int handle_lushr(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}


#define DO_LOGOP(op, member, utype, itype, tag) \
  itype a, b;                                   \
  var_t v1, v2, res;                            \
  v2 = pop_val(tag);                            \
  v1 = pop_val(tag);                            \
  a = (itype)v1.member;                         \
  b = (itype)v2.member;                         \
  res.member = (utype)(a op b);                 \
  push_val(res, tag);                           \
  return 1;

static int handle_iand(u1 *bc, java_class_t *cls) { DO_LOGOP(&, int_val, u4, i4, TAG_INT); }

static int handle_land(u1 *bc, java_class_t *cls) { DO_LOGOP(&, long_val, u8, i8, TAG_LONG); }

static int handle_ior(u1 *bc, java_class_t *cls) { DO_LOGOP(|, int_val, u4, i4, TAG_INT); }

static int handle_lor(u1 *bc, java_class_t *cls) { DO_LOGOP(|, long_val, u8, i8, TAG_LONG); }

static int handle_ixor(u1 *bc, java_class_t *cls) { DO_LOGOP(^, int_val, u4, i4, TAG_INT); }

static int handle_lxor(u1 *bc, java_class_t *cls) { DO_LOGOP(^, long_val, u8, i8, TAG_LONG); }

static int handle_iinc(u1 *bc, java_class_t *cls) {
  u1 idx = bc[1];
  int constant = (int)(char)bc[2];
  int v = (int)cur_thread->cur_frame->locals[idx].int_val;
  cur_thread->cur_frame->locals[idx].int_val = (u4)(v + constant);
  return 3;
}

static int handle_i2l(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_i2f(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_i2d(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_l2i(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_l2f(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_l2d(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_f2i(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_f2l(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_f2d(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_d2i(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_d2l(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_d2f(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_i2b(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_i2c(u1 *bc, java_class_t *cls) {
  var_t c = pop_val(TAG_INT);
  var_t res;
  char x;

  x = (char)c.int_val;
  res.int_val = (int)(u1)x;
  push_val(res, TAG_CHAR);

  return 1;
}

static int handle_i2s(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_lcmp(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_fcmpl(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_fcmpg(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dcmpl(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_dcmpg(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

#define DO_IF0(op, member, type, tag)    \
  i2 offset = (i2)GET_2B_IDX(bc);        \
  var_t v = pop_val(tag);                \
  if ((type)v.member op 0) {             \
    cur_thread->cur_frame->pc += offset; \
    return -ESHOULD_BRANCH;              \
  }                                      \
  return 3;

static int handle_ifeq(u1 *bc, java_class_t *cls) { DO_IF0(==, int_val, i4, TAG_INT); }

static int handle_ifne(u1 *bc, java_class_t *cls) { DO_IF0(!=, int_val, i4, TAG_INT); }

static int handle_iflt(u1 *bc, java_class_t *cls) { DO_IF0(<, int_val, i4, TAG_INT); }

static int handle_ifge(u1 *bc, java_class_t *cls) { DO_IF0(>=, int_val, i4, TAG_INT); }

static int handle_ifgt(u1 *bc, java_class_t *cls) { DO_IF0(>, int_val, i4, TAG_INT); }

static int handle_ifle(u1 *bc, java_class_t *cls) { DO_IF0(<=, int_val, i4, TAG_INT); }

#define DO_ICMP(op, member, itype, tag)  \
  i2 offset = (i2)GET_2B_IDX(bc);        \
  var_t v2 = pop_val(tag);               \
  var_t v1 = pop_val(tag);               \
  itype a = (itype)v1.member;            \
  itype b = (itype)v2.member;            \
  if (a op b) {                          \
    cur_thread->cur_frame->pc += offset; \
    return -ESHOULD_BRANCH;              \
  }                                      \
  return 3;

static int handle_if_icmpeq(u1 *bc, java_class_t *cls) { DO_ICMP(==, int_val, i4, TAG_INT); }

static int handle_if_icmpne(u1 *bc, java_class_t *cls) { DO_ICMP(!=, int_val, i4, TAG_INT); }

static int handle_if_icmplt(u1 *bc, java_class_t *cls) { DO_ICMP(<, int_val, i4, TAG_INT); }

static int handle_if_icmpge(u1 *bc, java_class_t *cls) { DO_ICMP(>=, int_val, i4, TAG_INT); }

static int handle_if_icmpgt(u1 *bc, java_class_t *cls) { DO_ICMP(>, int_val, i4, TAG_INT); }

static int handle_if_icmple(u1 *bc, java_class_t *cls) { DO_ICMP(<=, int_val, i4, TAG_INT); }

#define DO_ACMP(op)                      \
  i2 offset = (i2)GET_2B_IDX(bc);        \
  var_t v2 = pop_val(TAG_OBJ);           \
  var_t v1 = pop_val(TAG_OBJ);           \
  if (v1.ptr_val op v2.ptr_val) {        \
    cur_thread->cur_frame->pc += offset; \
    return -ESHOULD_BRANCH;              \
  }                                      \
  return 3;

static int handle_if_acmpeq(u1 *bc, java_class_t *cls) { DO_ACMP(==); }

static int handle_if_acmpne(u1 *bc, java_class_t *cls) { DO_ACMP(!=); }

static int handle_goto(u1 *bc, java_class_t *cls) {
  i2 offset = (i2)GET_2B_IDX(bc);
  cur_thread->cur_frame->pc += offset;
  return -ESHOULD_BRANCH;
}

static int handle_jsr(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ret(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_tableswitch(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_lookupswitch(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

#define DO_VAL_RETURN(tag)                                  \
  var_t ret = pop_val(tag);                                 \
  op_stack_t *prev = cur_thread->cur_frame->prev->op_stack; \
  prev->oprs[++(prev->sp)] = ret;                           \
  if (hb_pop_frame(cur_thread) != 0) {                      \
    HB_ERR("Could not pop stack frame in %s", __func__);    \
    return -1;                                              \
  }                                                         \
  return -ESHOULD_RETURN;

static int handle_ireturn(u1 *bc, java_class_t *cls) { DO_VAL_RETURN(TAG_INT); }

static int handle_lreturn(u1 *bc, java_class_t *cls) { DO_VAL_RETURN(TAG_LONG); }

static int handle_freturn(u1 *bc, java_class_t *cls) { DO_VAL_RETURN(TAG_FLOAT); }

static int handle_dreturn(u1 *bc, java_class_t *cls) { DO_VAL_RETURN(TAG_DOUBLE); }

static int handle_areturn(u1 *bc, java_class_t *cls) { DO_VAL_RETURN(TAG_OBJ); }

static int handle_return(u1 *bc, java_class_t *cls) {
  // TODO: check to make sure it's a void method we're
  // returning from
  if (hb_pop_frame(cur_thread) != 0) {
    HB_ERR("Could not pop stack frame in %s", __func__);
    return -1;
  }

  return -ESHOULD_RETURN;
}

static int handle_putstatic(u1 *bc, java_class_t *cls) {
  field_info_t *fi = NULL;
  var_t val;
  u2 idx;

  idx = GET_2B_IDX(bc);

  if (!FIELD_IS_RESOLVED((void *)cls->const_pool[idx])) {
    fi = hb_find_field(idx, cls, NULL);

    if (!fi) {
      HB_ERR("Could not resolve field ref in %s", __func__);
      return -1;
    }

    hb_resolve_static_field(fi, cls, idx);

  } else {
    fi = (field_info_t *)MASK_RESOLVED_BIT(cls->const_pool[idx]);
  }

  if (!(fi->acc_flags & ACC_STATIC)) {
    hb_throw_and_create_excp(EXCP_INCMP_CLS_CH);
    return -ESHOULD_BRANCH;
  }

  // TODO: tag should match the field type
  val = pop_val(TAG_ANY);

  // TODO: should do type checking here
  if (!fi->value) {
    HB_ERR("Static field is not bound in %s", __func__);
    return -1;
  }

  *(fi->value) = val;

  return 3;
}

static int handle_getfield(u1 *bc, java_class_t *cls) {
  field_info_t *fi = NULL;
  obj_ref_t *oref = NULL;
  native_obj_t *obj = NULL;
  var_t oval;
  int val_offset;
  u2 idx;

  idx = GET_2B_IDX(bc);

  oval = pop_val(TAG_OBJ);
  oref = oval.obj;

  if (!oref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  obj = (native_obj_t *)oref->heap_ptr;

  BC_DEBUG("Getting field from obj in class %s", hb_get_class_name(obj->class));

  if (!FIELD_IS_RESOLVED(cls->const_pool[idx])) {
    fi = hb_find_field(idx, cls, NULL);
    if (!fi) {
      HB_ERR("Could not resolve field ref in %s", __func__);
      return -1;
    }
    hb_resolve_field(fi, obj, cls, idx);
  }

  val_offset = (int)(MASK_RESOLVED_BIT(cls->const_pool[idx]));
  if (!(val_offset >= 0 && val_offset < obj->field_count)) {
    HB_ERR(
        "%s: Cannot resolve field, index out of bounds. "
        "Got idx = %d except length = %d",
        __func__, val_offset, obj->field_count);
    return -1;
  }
  fi = obj->field_infos[val_offset];

  if (fi->acc_flags & ACC_STATIC) {
    hb_throw_and_create_excp(EXCP_INCMP_CLS_CH);
    return -ESHOULD_BRANCH;
  }

  BC_DEBUG("Getting field %s in %s (name_idx=%d) (offset is %d)", hb_get_const_str(fi->name_idx, fi->owner), hb_get_class_name(fi->owner),
      fi->name_idx, val_offset);

  // TODO: Is this good enough for tag?
  var_t v = obj->fields[val_offset];
  push_val(v, v.tag);

  return 3;
}

static int handle_putfield(u1 *bc, java_class_t *cls) {
  field_info_t *fi = NULL;
  obj_ref_t *oref = NULL;
  native_obj_t *obj = NULL;
  int val_offset;
  var_t oval;
  var_t val;
  u2 idx;

  idx = GET_2B_IDX(bc);

  // TODO: `val` tag type
  val = pop_val(TAG_ANY);
  oval = pop_val(TAG_OBJ);
  oref = oval.obj;

  printf("val: %p, oval: %p, oref: %p\n", val, oval, oref);

  if (!oref) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  obj = (native_obj_t *)oref->heap_ptr;
  if (!obj) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  BC_DEBUG("Putting field in obj in class %s", hb_get_class_name(obj->class));

  if (!FIELD_IS_RESOLVED(cls->const_pool[idx])) {
    fi = hb_find_field(idx, cls, NULL);
    if (!fi) {
      HB_ERR("Could not resolve field ref in %s", __func__);
      return -1;
    }
    hb_resolve_field(fi, obj, cls, idx);
  }

  val_offset = (int)(MASK_RESOLVED_BIT(cls->const_pool[idx]));
  if (!(val_offset >= 0 && val_offset < obj->field_count)) {
    HB_ERR(
        "%s: Cannot resolve field, index out of bounds. "
        "Got idx = %d except length = %d",
        __func__, val_offset, obj->field_count);
    return -1;
  }
  fi = obj->field_infos[val_offset];

  if (fi->acc_flags & ACC_STATIC) {
    hb_throw_and_create_excp(EXCP_INCMP_CLS_CH);
    return -ESHOULD_BRANCH;
  }

  BC_DEBUG("Putting field %s in %s (name_idx=%d)", hb_get_const_str(fi->name_idx, fi->owner), hb_get_class_name(fi->owner), fi->name_idx);

  obj->fields[val_offset] = val;

  return 3;
}


static int __invokespecial(u1 *bc, java_class_t *cls, u1 type) {
  method_info_t *mi = NULL;
  u2 idx;

  idx = GET_2B_IDX(bc);

  mi = hb_resolve_method(idx, cls, NULL);

  if (!mi) {
    HB_ERR("Could not resolve method ref in %s", __func__);
    return -1;
  }

  BC_DEBUG("Method resolved as %s (owner=%s)", hb_get_const_str(mi->name_idx, mi->owner), hb_get_class_name(mi->owner));

  // if this is virtual, we need to figure out
  // 1) where the object ref is (param count)
  // 2) if the class of the object ref (or superclasses)
  // overrides the method. If so, we invoke those
  //
  if (type == ST_INVOKE_VIRT) {
    method_info_t *mi2 = NULL;
    op_stack_t *stack = cur_thread->cur_frame->op_stack;
    int nwide = 0;
    int pcount = hb_get_parm_count_from_method(cur_thread, mi, type, &nwide);
    obj_ref_t *ref = stack->oprs[stack->sp - pcount + nwide + 1].obj;
    if (!ref) {
      HB_ERR("Bad object ref in invoke virt!");
      hb_throw_and_create_excp(EXCP_NULL_PTR);
      return -ESHOULD_BRANCH;
    }
    native_obj_t *obj = (native_obj_t *)ref->heap_ptr;

    mi2 = hb_find_method_by_desc(hb_get_const_str(mi->name_idx, mi->owner), hb_get_const_str(mi->desc_idx, mi->owner), obj->class);

    // did we find an overriding method?
    if (mi2) {
      mi = mi2;
    }
  }

  if (mi->acc_flags & ACC_NATIVE) {
    if (hb_handle_native(mi, cls, type) != 0) {
      HB_ERR("Could not handle native method");
      return -1;
    }
    // native functions do not push a new stack frame,
    // so we bump our pc here
    cur_thread->cur_frame->pc += 3;
  } else {
    if (hb_push_frame_by_method(cur_thread, mi) != 0) {
      HB_ERR("Could not push method frame");
      return -1;
    }

    // we now copy params into the local vars for the method.
    // instance methods require that the first parameter
    // is the object on the current operand stack. This is
    // the "this" pointer.
    if (hb_setup_method_parms(cur_thread, mi, type) != 0) {
      HB_ERR("Could not setup method parameters");
      return -1;
    }

    // We have to special case the PC bump for function
    // invocation. We don't want to pass along an increment
    // since we're mutating the frame structure!
    cur_thread->cur_frame->prev->pc += 3;
  }

  return 0;
}

static int handle_invokespecial(u1 *bc, java_class_t *cls) { return __invokespecial(bc, cls, ST_INVOKE_SPECIAL); }

/*
 * TODO: these are actually subtly different when
 * it comes to method resolution. Need to fix.
 */
static int handle_invokevirtual(u1 *bc, java_class_t *cls) { return __invokespecial(bc, cls, ST_INVOKE_VIRT); }

static int handle_invokestatic(u1 *bc, java_class_t *cls) { return __invokespecial(bc, cls, ST_INVOKE_STATIC); }

static int handle_invokeinterface(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_invokedynamic(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_new(u1 *bc, java_class_t *cls) {
  java_class_t *target_cls = NULL;
  obj_ref_t *oa = NULL;
  native_obj_t *aobj = NULL;
  var_t ret;
  u2 idx;

  /* get index into the constant pool of the target class */
  idx = GET_2B_IDX(bc);

  /* load and initialize the class (if not already) */
  target_cls = hb_resolve_class(idx, cls);
  if (!target_cls) {
    HB_ERR("Could not resolve class ref in %s", __func__);
    return -1;
  }

  oa = gc_obj_alloc(target_cls);
  if (!oa) {
    hb_throw_and_create_excp(EXCP_OOM);
    return -ESHOULD_BRANCH;
  }

  ret.obj = oa;
  aobj = (native_obj_t *)oa->heap_ptr;
  aobj->class = target_cls;

  BC_DEBUG("Allocated new object %p in %s", ret.obj, __func__);
  push_val(ret, TAG_OBJ);

  return 3;
}

// WRITE ME
static int handle_newarray(u1 *bc, java_class_t *cls) {
  obj_ref_t *oa = NULL;
  var_t len = pop_val(TAG_INT);
  var_t ret;
  u2 atype;

  atype = bc[1];

  if (len.int_val < 0) {
    hb_throw_and_create_excp(EXCP_NEG_ARR_SIZE);
    return -ESHOULD_BRANCH;
  }

  oa = gc_array_alloc(atype, len.int_val);

  if (!oa) {
    hb_throw_and_create_excp(EXCP_OOM);
    return -ESHOULD_BRANCH;
  }

  ret.obj = oa;

  BC_DEBUG("Allocated new array at %p in %s", ret.obj, __func__);
  push_val(ret, TAG_OBJ);

  return 2;
}

static int handle_anewarray(u1 *bc, java_class_t *cls) {
  java_class_t *target_cls = NULL;
  obj_ref_t *oa = NULL;
  native_obj_t *aobj = NULL;
  var_t len = pop_val(TAG_INT);
  var_t ret;
  u2 idx;

  idx = GET_2B_IDX(bc);

  /* load and initialize the class (if not already) */
  target_cls = hb_resolve_class(idx, cls);

  if (!target_cls) {
    HB_ERR("Could not resolve class ref in %s", __func__);
    return -1;
  }

  if (len.int_val < 0) {
    hb_throw_and_create_excp(EXCP_NEG_ARR_SIZE);
    return -ESHOULD_BRANCH;
  }

  oa = gc_array_alloc(T_REF, len.int_val);

  if (!oa) {
    hb_throw_and_create_excp(EXCP_OOM);
    return -ESHOULD_BRANCH;
  }

  aobj = (native_obj_t *)oa->heap_ptr;

  aobj->class = target_cls;

  ret.obj = oa;

  BC_DEBUG("Allocated new array at %p in %s", ret.obj, __func__);
  push_val(ret, TAG_OBJ);

  return 3;
}

// WRITE ME
static int handle_arraylength(u1 *bc, java_class_t *cls) {
  // printf("===============\n");
  var_t array = pop_val(TAG_OBJ);

  // check for null-ness
  if (!array.obj) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  // I think this works right
  native_obj_t *aobj = (native_obj_t *)array.obj->heap_ptr;
  // check that its an array
  if (!aobj->flags.array.isarray) {
    hb_throw_and_create_excp(EXCP_RUNTIME);
    return -ESHOULD_BRANCH;
  }

  int len = aobj->flags.array.length;

  var_t r;
  r.int_val = len;
  // printf("%d\n", len);
  push_val(r, TAG_INT);

  // printf("===============\n");
  return 1;
}

// WRITE ME
static int handle_athrow(u1 *bc, java_class_t *cls) {
  var_t v = pop_val(TAG_OBJ);
  obj_ref_t *obj = v.obj;

  // check for null
  if (!obj) {
    hb_throw_and_create_excp(EXCP_NULL_PTR);
    return -ESHOULD_BRANCH;
  }

  hb_throw_exception(obj);
  return -ESHOULD_BRANCH;
}

static int handle_checkcast(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_instanceof(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_monitorenter(u1 *bc, java_class_t *cls) {
  pop_val(TAG_ANY);
  BC_DEBUG("WARNING: entering dummy monitor");
  return 1;
}

static int handle_monitorexit(u1 *bc, java_class_t *cls) {
  pop_val(TAG_ANY);
  BC_DEBUG("WARNING: exiting dummy monitor");
  return 1;
}

static int handle_wide(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_multianewarray(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_ifnull(u1 *bc, java_class_t *cls) {
  i2 offset = (i2)GET_2B_IDX(bc);
  var_t v = pop_val(TAG_OBJ);
  if (v.obj == NULL) {
    cur_thread->cur_frame->pc += offset;
    return -ESHOULD_BRANCH;
  }

  return 3;
}

static int handle_ifnonnull(u1 *bc, java_class_t *cls) {
  i2 offset = (i2)GET_2B_IDX(bc);
  var_t v = pop_val(TAG_OBJ);
  if (v.obj != NULL) {
    cur_thread->cur_frame->pc += offset;
    return -ESHOULD_BRANCH;
  }

  return 3;
}

static int handle_goto_w(u1 *bc, java_class_t *cls) {
  i4 offset = (i4)GET_4B_IDX(bc);
  cur_thread->cur_frame->pc += offset;
  return -ESHOULD_BRANCH;
}

static int handle_jsr_w(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_breakpoint(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_impdep1(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}

static int handle_impdep2(u1 *bc, java_class_t *cls) {
  HB_ERR("%s NOT IMPLEMENTED", __func__);
  return -1;
}


static int handle_getstatic(u1 *bc, java_class_t *cls) {
  field_info_t *fi = NULL;
  u2 idx;

  idx = GET_2B_IDX(bc);

  if (!FIELD_IS_RESOLVED((void *)cls->const_pool[idx])) {
    fi = hb_find_field(idx, cls, NULL);

    if (!fi) {
      HB_ERR("Could not resolve field ref in %s", __func__);
      return -1;
    }

    hb_resolve_static_field(fi, cls, idx);

  } else {
    fi = (field_info_t *)MASK_RESOLVED_BIT(cls->const_pool[idx]);
  }

  if (!fi->value) {
    HB_ERR("Value is not initialized in %s!", __func__);
    return -1;
  }

  /* push the resolved field on the stack */
  var_t v = *(fi->value);
  push_val(v, v.tag);

  return 3;
}


int hb_invoke_ctor(obj_ref_t *oref) {
  if (hb_push_ctor_frame(cur_thread, oref) != 0) {
    HB_ERR("Could not push ctor frame");
    return -1;
  }

  if (hb_exec_method(cur_thread) != 0) {
    HB_ERR("Could not invoke ctor");
    return -1;
  }

  return 0;
}

#include <opcode_map.h>

int hb_instr_repr(jthread_t *t, char *buf, size_t buflen) {
  u1 *bc_ptr = t->cur_frame->minfo->code_attr->code;
  u1 opcode = bc_ptr[t->cur_frame->pc];
  snprintf(buf, buflen, "%s", mnemonics[opcode]);
  return 0;
}

int hb_exec_one(jthread_t *t) {
  u1 *bc_ptr = NULL;
  java_class_t *cls = t->cur_frame->cls;
  const char *method_nm = NULL;
  const char *method_desc = NULL;

  method_nm = hb_get_const_str(t->cur_frame->minfo->name_idx, cls);
  method_desc = hb_get_const_str(t->cur_frame->minfo->desc_idx, cls);

  int ret;

  bc_ptr = t->cur_frame->minfo->code_attr->code;

  u1 opcode = bc_ptr[t->cur_frame->pc];

  BC_DEBUG(
      "{PC:%02x} [%s::%s] Encountered OP (0x%02x) (%s)", t->cur_frame->pc, hb_get_class_name(cls), method_nm, opcode, mnemonics[opcode]);

  ret = handlers[opcode](&bc_ptr[t->cur_frame->pc], cls);

  // see if its time to GC (only if we still have a frame)
  if (t->cur_frame && gc_should_collect(t)) {
    gc_collect(t);
  }

#if DEBUG == 1
  // if we pop off main frame from return, can't do this
  if (t->cur_frame) {
    hb_dump_op_stack();
    hb_dump_locals();
  }
#endif

  return ret;
}

//
// This runs a method to completion,
// ignoring any breakpoints. Note that
// we assume this will *not* be used to
// run main(), since it will not expect
// any null frames. We also assume that
// the method's stack frame has already
// been pushed.
//
int hb_exec_method(jthread_t *t) {
  java_class_t *cls = t->cur_frame->cls;
  const char *method_nm = NULL;
  const char *method_desc = NULL;
  stack_frame_t *orig_frame = t->cur_frame->prev;

  method_nm = hb_get_const_str(t->cur_frame->minfo->name_idx, cls);
  method_desc = hb_get_const_str(t->cur_frame->minfo->desc_idx, cls);

  BC_DEBUG("Executing method (%s) for class (%s)", method_nm, hb_get_class_name(cls));

  while (1) {
    int ret = hb_exec_one(t);

    if (ret == -1) {
      HB_ERR("Could not execute method");
      exit(EXIT_FAILURE);
    } else if (ret == -ESHOULD_RETURN) {
      // we check against the orig frame's prev
      // becuase we've already popped off in the return
      // instr handler
      if (t->cur_frame == orig_frame) break;
    } else if (ret == -ESHOULD_BRANCH) {
      // these handlers will set PC explicitly,
      // so we should not increment or decrement here
    } else {
      t->cur_frame->pc += ret;
    }
  }

  return 0;
}

