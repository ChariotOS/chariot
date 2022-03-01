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

#include <constants.h>
#include <hawkbeans.h>
#include <types.h>

#if DEBUG_CLASS == 1
#define CL_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define CL_DEBUG(fmt, args...)
#endif

/* Access and property modifiers */
#define ACC_PUBLIC 0x0001
#define ACC_PRIVATE 0x0002
#define ACC_PROTECTED 0x0004
#define ACC_STATIC 0x0008
#define ACC_FINAL 0x0010
#define ACC_SUPER 0x0020
#define ACC_VOLATILE 0x0040
#define ACC_TRANSIENT 0x0080
#define ACC_INTERFACE 0x0200
#define ACC_ABSTRACT 0x0400
#define ACC_SYNTHETIC 0x1000
#define ACC_ANNOTATION 0x2000
#define ACC_ENUM 0x4000

#define JAVA_MAGIC 0xCAFEBABE

#define MARK_RESOLVED(x) ((uint64_t)(x) | (1UL << 63u))
#define IS_RESOLVED(x) ((uint64_t)(x) & (1UL << 63u))
#define MASK_RESOLVED_BIT(x) ((uint64_t)(x) & ~(1UL << 63u))

#define MARK_FIELD_RESOLVED(x) MARK_RESOLVED(x)
#define FIELD_IS_RESOLVED(x) IS_RESOLVED(x)

#if DEBUG_CLASS == 1
#define CL_DEBUG(fmt, args...) HB_DEBUG(fmt, ##args)
#else
#define CL_DEBUG(fmt, args...)
#endif

enum ref_types {
  OBJ_OBJ,
  OBJ_ARRAY,
};

typedef struct obj_ref {
  u8 heap_ptr;
  u1 type;
} obj_ref_t;

#define TAG_BAD 0
#define TAG_CHAR 1u
#define TAG_SHORT 2u
#define TAG_INT 4u
#define TAG_INT_LIKE (TAG_CHAR | TAG_SHORT | TAG_INT)
#define TAG_LONG 8u
#define TAG_FLOAT 16u
#define TAG_DOUBLE 32u
#define TAG_PTR 64u
#define TAG_OBJ 128u
#define TAG_ANY 0xFFu

#define TAG_MIN TAG_CHAR
#define TAG_MAX TAG_ANY

#define CONST_TO_TAG(c) \
  (((c) == CONSTANT_Integer) ? TAG_INT : \
  ((c) == CONSTANT_Long) ? TAG_LONG : \
  ((c) == CONSTANT_Float) ? TAG_FLOAT : \
  ((c) == CONSTNAT_Double) ? TAG_DOUBLE : \
  ((c) == CONSTANT_String || (c) == CONSTANT_Utf8) ? TAG_OBJ : \
   TAG_BAD)

#define TAG_STR(t) \
  ((t) == TAG_BAD) ? "ERROR" : \
  ((t) == TAG_CHAR) ? "char" : \
  ((t) == TAG_SHORT) ? "short" : \
  ((t) == TAG_INT) ? "int" : \
  ((t) == TAG_LONG) ? "long" : \
  ((t) == TAG_FLOAT) ? "float" : \
  ((t) == TAG_DOUBLE) ? "double" : \
  ((t) == TAG_PTR) ? "ptr" : \
  ((t) == TAG_OBJ) ? "object ref" : \
  ((t) == TAG_ANY) ? "any (INVALID)" : \
   "OUT OF RANGE"

#define EXPECT_TAG(__VAR, __TAG) \
  if ((__VAR).tag != __TAG) { \
	  HB_ERR("%s: bad tag, got '%s', expected '%s'", __func__, TAG_STR((__VAR).tag), TAG_STR(__TAG)); \
	}

typedef struct variable {
  u1 tag;
  union {
    u1 char_val;
    u2 short_val;
    u4 int_val;
    u8 long_val;
    f4 float_val;
    d8 dbl_val;
    u8 ptr_val;
    struct obj_ref *obj;
  };
} var_t;

typedef struct const_pool_info {
  u1 tag;
  u1 info[0];
} const_pool_info_t;

struct line_tbl_entry {
  u2 start_pc;
  u2 line_num;
};

struct local_var_tbl_entry {
  u2 start_pc;
  u2 len;
  u2 name_idx;
  u2 desc_idx;
  u2 idx;
};

typedef struct attr_info {
  u2 name_idx;
  u4 len;
  union {
    struct {
      u2 srcfile_idx;
    } __attribute__((packed)) src_file;
    struct {
      u2 dbg_ext[0];
    } __attribute__((packed)) src_dbg_ext;
    struct {
      u2 line_tbl_len;
      struct line_tbl_entry entries[0];
    } __attribute__((packed)) line_num_tbl;
    struct {
      u2 local_var_tbl_len;
      struct local_var_tbl_entry entries[0];
    } __attribute__((packed)) local_var_tbl;
  } __attribute__((packed));
} __attribute__((packed)) attr_info_t;

typedef struct excp_table {
  u2 start_pc;
  u2 end_pc;
  u2 handler_pc;
  u2 catch_type;
} excp_table_t;

typedef struct code_attr {
  u2 attr_name_idx;
  u4 attr_len;
  u2 max_stack;
  u2 max_locals;
  u4 code_len;
  u1 *code;
  u2 excp_table_len;
  excp_table_t *excp_table;
  u2 attr_count;
  attr_info_t **attrs;
} code_attr_t;

typedef struct method_info {
  u2 acc_flags;
  u2 name_idx;
  u2 desc_idx;
  u2 attr_count;

  struct java_class *owner;
  code_attr_t *code_attr;

  attr_info_t *line_info;
  attr_info_t *lvar_info;

  char *name;
  char **args;
  char *ret_type;
  char *desc;
  int nargs;

} method_info_t;

typedef struct field_info {
  u2 acc_flags;
  u2 name_idx;
  u2 desc_idx;
  u2 attr_count;

  char *desc_str;
  char *name_str;

  attr_info_t *attrs;

  // these only apply to static fields
  struct java_class *owner;
  const_pool_info_t *cpe;
  u1 has_const;
  var_t *value;

} field_info_t;

/* class must be in one of these states */
typedef enum class_status {
  CLS_NONE,
  CLS_LOADED,
  CLS_PREPPED,
  CLS_INITED,
} class_stat_t;

/* structure of java class file */
typedef struct java_class {
  u4 magic;
  u2 minor_version;
  u2 major_version;
  u2 const_pool_count;
  const_pool_info_t **const_pool;
  u2 acc_flags;
  u2 this;
  u2 super;
  u2 interfaces_count;
  u2 *interfaces;  //[interfaces_count];
  u2 fields_count;
  field_info_t *fields;  //[fields_count];
  u2 methods_count;
  method_info_t *methods;  //[methods_count];
  u2 attr_count;
  attr_info_t **attrs;  //[attributes_count];

  class_stat_t status;

  var_t *field_vals;

  const char *name;
  const char *src_file;
  char **src_lines;
  unsigned num_src_lines;

} java_class_t;

#include <list.h>

typedef struct native_object {
  /* these are unused if this is allocated */
  int order;

  struct list_head link;  // for free list accounting

  java_class_t *class;

  union {
    u2 val;

    struct _obj {
      u1 isarray : 1;
      u1 gc_mark : 1;
      u2 pad : 14;
    } obj __attribute__((packed));

    struct _arr {
      u1 isarray : 1;
      u1 gc_mark : 1;
      u1 type : 5;
      u2 length : 9;
    } array __attribute__((packed));

  } flags __attribute__((packed));

  u2 field_count;

  var_t *fields;

  // used to cross-check during field resolution
  field_info_t **field_infos;

} native_obj_t;

extern java_class_t *g_java_object_class;

const char *hb_get_const_str(u2 idx, struct java_class *cls);
const char *hb_get_class_name(struct java_class *cls);

/* class resolution */
java_class_t *hb_resolve_class(u2 const_idx, java_class_t *src_cls);

/* field resolution */
field_info_t *hb_find_field(u2 idx, java_class_t *src_cls,
                            java_class_t *target_cls);
int hb_resolve_field(field_info_t *f, native_obj_t *obj, java_class_t *cls,
                     u2 const_idx);
int hb_resolve_static_field(field_info_t *f, java_class_t *cls, u2 const_idx);

/* methods */
method_info_t *hb_find_method_by_desc(const char *mname, const char *mdesc,
                                      java_class_t *cls);
method_info_t *hb_get_ctor_minfo(java_class_t *cls);
method_info_t *hb_resolve_method(u2 const_idx, java_class_t *src_cls,
                                 java_class_t *target_cls);
int hb_get_method_idx(const char *name, java_class_t *cls);

static inline char **hb_get_method_args(method_info_t *m) { return m->args; }

static inline char *hb_get_method_name(method_info_t *m) { return m->name; }

static inline char *hb_get_method_return_type(method_info_t *m) {
  return m->ret_type;
}

/* class prep */
int hb_prep_class(java_class_t *cls);
int hb_init_class(java_class_t *cls);

/* class map */
int hb_classmap_init(void);
int hb_class_is_loaded(const char *class_nm);
void hb_add_class(const char *class_nm, java_class_t *cls);
java_class_t *hb_get_class(const char *class_nm);
java_class_t *hb_get_or_load_class(const char *class_nm);

/* instance variables */
int hb_setup_obj_fields(native_obj_t *obj, java_class_t *cls);
int hb_get_obj_field_count(java_class_t *cls);

/* supers */
const char *hb_get_super_class_nm(java_class_t *cls);
java_class_t *hb_get_super_class(java_class_t *cls);

struct nk_hashtable *hb_get_classmap(void);

/* utility funcs */
static inline int hb_get_field_count(java_class_t *cls) {
  return cls->fields_count;
}

static inline int hb_get_method_count(java_class_t *cls) {
  return cls->methods_count;
}

static inline int hb_is_interface(java_class_t *cls) {
  return cls->acc_flags & ACC_INTERFACE;
}

static inline int hb_is_abstract(java_class_t *cls) {
  return cls->acc_flags & ACC_ABSTRACT;
}

static inline int hb_use_super_sem(java_class_t *cls) {
  return cls->acc_flags & ACC_SUPER;
}

static inline int hb_is_public(java_class_t *cls) {
  return cls->acc_flags & ACC_PUBLIC;
}

static inline int hb_get_max_locals(java_class_t *cls, int method_idx) {
  return cls->methods[method_idx].code_attr->max_locals;
}

static inline int hb_get_max_oprs(java_class_t *cls, int method_idx) {
  return cls->methods[method_idx].code_attr->max_stack;
}
