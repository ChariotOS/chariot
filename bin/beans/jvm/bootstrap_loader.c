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
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>

#include <hawkbeans.h>
#include <types.h>
#include <constants.h>
#include <class.h>

#include <arch/x64-linux/bootstrap_loader.h>
#include <arch/x64-linux/util.h>

#define GET_AND_INC(field, sz) \
  cls->field = get_u##sz(clb); \
  clb += sz

#define PARSE_AND_CHECK(count, func, str) \
  if (cls->count > 0) {                   \
    skip = 0;                             \
    if (func(clb, cls, &skip) != 0) {     \
      HB_ERR("Could not parse %s", str);  \
      return -1;                          \
    }                                     \
    clb += skip;                          \
  }

static const char* typeid_to_str[] = {
    ['B'] = "byte",
    ['C'] = "char",
    ['D'] = "double",
    ['F'] = "float",
    ['I'] = "int",
    ['J'] = "long",
    ['L'] = "reference",
    ['S'] = "short",
    ['Z'] = "boolean",
    ['['] = "reference",
};


const char* search_paths[] = {
    // "/usr/src/java",
    "/usr/src",
    // "/usr/java/testcode",
};


#define MAX_FIELD_LEN 1024

#define MAX_CHILD 32

typedef enum {
  CST_PARM,
  CST_FIELD,
  CST_METHOD,
  CST_RET,
  CST_VOID,
  CST_BASE,
  CST_ARRAY,
  CST_OBJ,
} cst_type_t;

struct cst_node {
  cst_type_t type;
  struct cst_node* children[MAX_CHILD];
  int num_children;
  const char* str_repr;
};

static struct cst_node* cst_new_node(cst_type_t type, const char* str_repr) {
  struct cst_node* node = malloc(sizeof(*node));
  if (!node) return NULL;
  memset(node, 0, sizeof(*node));

  node->type = type;
  node->num_children = 0;
  node->str_repr = strndup(str_repr, MAX_FIELD_LEN);

  return node;
}

static inline void cst_add_child(struct cst_node* root, struct cst_node* child) { root->children[root->num_children++] = child; }

static void cst_push(struct cst_node* root, cst_type_t type, const char* str_repr) {
  struct cst_node* node = cst_new_node(type, str_repr);
  cst_add_child(root, node);
}


/*
 * See https://docs.oracle.com/javase/specs/jvms/se7/html/jvms-4.html#jvms-4.3.2-200
 */
static void parse_field_desc(char** desc, struct cst_node* root, java_class_t* cls) {
  switch (**desc) {
    case 'L': {
      char* end = strchr(*desc, ';');
      char* class = strtok(++(*desc), ";");
      char* cursor;
      while ((cursor = strchr(class, '/')) != NULL)
        *cursor = '.';
      cst_push(root, CST_OBJ, class);
      *end = ';';
      *desc = strchr(*desc, ';') + 1;
      break;
    }
    case '[': {
      (*desc)++;
      struct cst_node* member = cst_new_node(CST_ARRAY, "array");
      cst_add_child(root, member);
      struct cst_node* field = cst_new_node(CST_FIELD, "field");
      cst_add_child(member, field);
      parse_field_desc(desc, field, cls);
      break;
    }
    default: {
      char* type = (char*)typeid_to_str[**desc];
      cst_push(root, CST_BASE, type);
      (*desc)++;
      break;
    }
  }
}


static void parse_return_desc(char** desc, struct cst_node* root, java_class_t* cls) {
  struct cst_node* ret = cst_new_node(CST_RET, "return");
  cst_add_child(root, ret);

  if (**desc == 'V')
    cst_push(ret, CST_VOID, "void");
  else
    parse_field_desc(desc, ret, cls);
}


/*
 * Grammar:
 *
 * MethodDescriptor:
 *   (ParameterDescriptor*) ReturnDescriptor
 * ParameterDescriptor:
 *   FieldType
 * ReturnDescriptor:
 *   FieldType
 *   VoidDescriptor
 * VoidDsscriptor:
 *   V
 */
static struct cst_node* parse_method_desc(char** desc, java_class_t* cls) {
  struct cst_node* root = cst_new_node(CST_METHOD, "method");

  while (**desc != ')') {
    struct cst_node* parm = cst_new_node(CST_PARM, "param");
    cst_add_child(root, parm);
    struct cst_node* field = cst_new_node(CST_FIELD, "field");
    cst_add_child(parm, field);
    parse_field_desc(desc, field, cls);
  }

  (*desc)++;

  parse_return_desc(desc, root, cls);

  return root;
}


/*
 * Descriptors (method and field) according to the Java spec happen
 * to be in an LL(0) grammar, making them quite easy
 * to parse. We'll apply a dead-simple, top-down
 * recursive descent parser here. We emit a parse
 * tree (CST).
 *
 * It is up to the user of this to free memory.
 *
 * Grammar:
 *   Descriptor:
 *     MethodDescriptor
 *     FieldType
 */
static struct cst_node* parse_desc(char* desc, java_class_t* cls) {
  if (*desc == '(') {
    desc++;
    return parse_method_desc(&desc, cls);
  } else {
    struct cst_node* root = cst_new_node(CST_FIELD, "field");
    parse_field_desc(&desc, root, cls);
    return root;
  }
}


static char* construct_method_sig(char** args, char* ret_type, int nargs) {
  char buf[MAX_FIELD_LEN];
  memset(buf, 0, MAX_FIELD_LEN);

  strcat(buf, "(");

  for (int i = 0; i < nargs; i++) {
    if (i != 0) strcat(buf, ",");

    strcat(buf, args[i]);
  }

  strcat(buf, "):");
  strcat(buf, ret_type);

  return strndup(buf, MAX_FIELD_LEN);
}


static struct cst_node* cst_ret_type(struct cst_node* root) {
  if (root->type != CST_METHOD) return NULL;

  for (int i = 0; i < root->num_children; i++) {
    if (root->children[i]->type == CST_RET) return root->children[i];
  }

  return NULL;
}


static char* method_ret_type(struct cst_node* ret) {
  if (!ret) return NULL;

  if (!ret->children[0]) return NULL;

  return strndup(ret->children[0]->str_repr, MAX_FIELD_LEN);
}


static void cst_args(struct cst_node* root, int* nargs, struct cst_node** args) {
  for (int i = 0; i < root->num_children; i++) {
    if (root->children[i]->type == CST_PARM) {
      args[i] = root->children[i];
      (*nargs)++;
    }
  }
}


static void construct_field_sig(struct cst_node* root, char* buf) {
  struct cst_node* node;

  if (!root) {
    HB_ERR("Bad root given");
    return;
  }

  if (root->type != CST_FIELD) {
    HB_ERR("Not a field type : %d %s", root->type, root->str_repr);
    return;
  }

  node = root->children[0];

  if (!node) {
    HB_ERR("Field node has no children");
    return;
  }

  switch (node->type) {
    case CST_BASE:
      strncat(buf, node->str_repr, MAX_FIELD_LEN);
      break;
      ;
    case CST_OBJ:
      strncat(buf, node->str_repr, MAX_FIELD_LEN);
      break;
      ;
    case CST_ARRAY:
      construct_field_sig(node->children[0], buf);
      strncat(buf, "[]", MAX_FIELD_LEN);
      break;
    default:
      HB_ERR("Unknown non-terminal in field sig (%s)", node->str_repr);
      break;
      ;
  }
}


static char** method_args(struct cst_node** args, int n) {
  char** ret = NULL;

  if (!n) return NULL;

  ret = malloc(sizeof(char*) * n);

  for (int i = 0; i < n; i++) {
    char buf[MAX_FIELD_LEN];
    memset(buf, 0, MAX_FIELD_LEN);

    if (args[i]->type != CST_PARM) {
      HB_ERR("Not a parameter!");
      return NULL;
    }

    if (args[i]->children[0]->type == CST_FIELD) {
      construct_field_sig(args[i]->children[0], (char*)&buf);
    } else if (args[i]->children[0]->type == CST_VOID) {
      strncat(buf, args[i]->children[0]->str_repr, MAX_FIELD_LEN);
    } else {
      HB_ERR("Bad arg type");
      return NULL;
    }

    ret[i] = strndup(buf, MAX_FIELD_LEN);
  }

  return ret;
}



static void resolve_field_strings(field_info_t* fi, java_class_t* cls) {
  char buf[MAX_FIELD_LEN];
  memset(buf, 0, MAX_FIELD_LEN);
  char* desc_str = strndup((char*)hb_get_const_str(fi->desc_idx, cls), MAX_FIELD_LEN);
  struct cst_node* root = parse_desc(desc_str, cls);
  fi->name_str = (char*)hb_get_const_str(fi->name_idx, cls);
  construct_field_sig(root, (char*)&buf);
  fi->desc_str = strndup(buf, MAX_FIELD_LEN);
}


static void resolve_method_strings(method_info_t* mi, java_class_t* cls) {
  char* desc_str = strndup((char*)hb_get_const_str(mi->desc_idx, cls), MAX_FIELD_LEN);
  mi->name = (char*)hb_get_const_str(mi->name_idx, cls);
  struct cst_node* root = parse_desc(desc_str, cls);
  struct cst_node* ret_type = cst_ret_type(root);
  struct cst_node* args[32];
  memset(args, 0, sizeof(struct cst_node*) * 32);
  int nargs = 0;

  mi->ret_type = method_ret_type(ret_type);

  cst_args(root, &nargs, (struct cst_node**)args);

  mi->args = method_args(args, nargs);
  mi->desc = construct_method_sig(mi->args, mi->ret_type, nargs);
  mi->nargs = nargs;
}

#define MAX_CLASS_PATH 512

static FILE* open_in_dir(const char* dirname, const char* fname, const char* mode) {
  DIR* d = opendir(dirname);
  FILE* f = NULL;
  struct dirent* de;
  struct stat statbuf;
  char buf[MAX_CLASS_PATH];

  chdir(dirname);

	char cwd[128];
	getcwd(cwd, 128);
  if (!d) {
    HB_ERR("Could not open dir %s", dirname);
    return NULL;
  }


	f = fopen(fname, mode);
	if (f) goto out;

  // first check in this level
  while ((de = readdir(d))) {
    lstat(de->d_name, &statbuf);
    // is this another dir?
    if (!S_ISDIR(statbuf.st_mode)) {
      if (strncmp(de->d_name, fname, MAX_CLASS_PATH) == 0) {
        f = fopen(fname, mode);
        if (f) goto out;
      }

    } else {
      // ignore . and ..
      if (!strcmp(".", de->d_name) || !strcmp("..", de->d_name)) continue;
      f = open_in_dir(de->d_name, fname, mode);
      if (f) goto out;
    }
  }

out:
  chdir("..");
  closedir(d);
  return f;
}

static FILE* hb_open(const char* fname, const char* mode) {
  FILE* f = NULL;

  printf("hb_open(%s)\n", fname);
  // first check in top-level
  if ((f = fopen(fname, mode))) {
    return f;
  }

  for (int i = 0; i < sizeof(search_paths) / sizeof(char*); i++) {
    f = open_in_dir(search_paths[i], fname, mode);
    if (f) return f;
  }

  return NULL;
}

static u1* open_class_file(const char* path) {
  FILE* fd;
  struct stat s;
  void* cm = NULL;
  const char* suf = ".class";
  char buf[MAX_CLASS_PATH];

  // do we need to add the .class extension?
  if (!strstr(path, suf)) {
    snprintf(buf, MAX_CLASS_PATH, "%s%s", path, suf);
  } else {
    strncpy(buf, path, MAX_CLASS_PATH);
  }

  if ((fd = hb_open(buf, "r")) == NULL) {
    HB_ERR("Could not open file (%s): %s", path, strerror(errno));
    return NULL;
  }

  if (fstat(fileno(fd), &s) == -1) {
    HB_ERR("Could not stat file (%s): %s", path, strerror(errno));
    return NULL;
  }

  if ((cm = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fileno(fd), 0)) == MAP_FAILED) {
    HB_ERR("Could not map file (%s): %s", path, strerror(errno));
    return NULL;
  }

  return (u1*)cm;
}


static const_pool_info_t* parse_const_pool_entry(u1* ptr, u1* sz) {
  switch (*ptr) {
    case CONSTANT_Class: {
      CONSTANT_Class_info_t* c = malloc(sizeof(CONSTANT_Class_info_t));
      c->tag = *ptr;
      c->name_idx = get_u2(ptr + 1);
      *sz = 3;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Fieldref: {
      CONSTANT_Fieldref_info_t* c = malloc(sizeof(CONSTANT_Fieldref_info_t));
      c->tag = *ptr;
      c->class_idx = get_u2(ptr + 1);
      c->name_and_type_idx = get_u2(ptr + 3);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Methodref: {
      CONSTANT_Methodref_info_t* c = malloc(sizeof(CONSTANT_Methodref_info_t));
      c->tag = *ptr;
      c->class_idx = get_u2(ptr + 1);
      c->name_and_type_idx = get_u2(ptr + 3);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_InterfaceMethodref: {
      CONSTANT_InterfaceMethodref_info_t* c = malloc(sizeof(CONSTANT_InterfaceMethodref_info_t));
      c->tag = *ptr;
      c->class_idx = get_u2(ptr + 1);
      c->name_and_type_idx = get_u2(ptr + 3);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_String: {
      CONSTANT_String_info_t* c = malloc(sizeof(CONSTANT_String_info_t));
      c->tag = *ptr;
      c->str_idx = get_u2(ptr + 1);
      *sz = 3;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Integer: {
      CONSTANT_Integer_info_t* c = malloc(sizeof(CONSTANT_Integer_info_t));
      c->tag = *ptr;
      c->bytes = get_u4(ptr + 1);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Float: {
      CONSTANT_Float_info_t* c = malloc(sizeof(CONSTANT_Float_info_t));
      c->tag = *ptr;
      c->bytes = get_u4(ptr + 1);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Long: {
      CONSTANT_Long_info_t* c = malloc(sizeof(CONSTANT_Long_info_t));
      c->tag = *ptr;
      c->hi_bytes = get_u4(ptr + 1);
      c->lo_bytes = get_u4(ptr + 5);
      *sz = 9;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Double: {
      CONSTANT_Double_info_t* c = malloc(sizeof(CONSTANT_Double_info_t));
      c->tag = *ptr;
      c->hi_bytes = get_u4(ptr + 1);
      c->lo_bytes = get_u4(ptr + 5);
      *sz = 9;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_NameAndType: {
      CONSTANT_NameAndType_info_t* c = malloc(sizeof(CONSTANT_NameAndType_info_t));
      c->tag = *ptr;
      c->name_idx = get_u2(ptr + 1);
      c->desc_idx = get_u2(ptr + 3);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_Utf8: {
      CONSTANT_Utf8_info_t* c = NULL;
      u1 tag = *ptr;
      u2 len = get_u2(ptr + 1);
      c = malloc(sizeof(CONSTANT_Utf8_info_t) + len + 1);
      c->tag = tag;
      c->len = len;
      *sz = 3 + len;
      memset(c->bytes, 0, len + 1);
      memcpy(c->bytes, ptr + 3, len);
      return (const_pool_info_t*)c;
    }
    case CONSTANT_MethodHandle: {
      CONSTANT_MethodHandle_info_t* c = malloc(sizeof(CONSTANT_MethodHandle_info_t));
      c->tag = *ptr;
      c->ref_kind = *(ptr + 1);
      c->ref_idx = get_u2(ptr + 2);
      *sz = 4;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_MethodType: {
      CONSTANT_MethodType_info_t* c = malloc(sizeof(CONSTANT_MethodType_info_t));
      c->tag = *ptr;
      c->desc_idx = get_u2(ptr + 1);
      *sz = 3;
      return (const_pool_info_t*)c;
    }
    case CONSTANT_InvokeDynamic: {
      CONSTANT_InvokeDynamic_info_t* c = malloc(sizeof(CONSTANT_InvokeDynamic_info_t));
      c->tag = *ptr;
      c->bootstrap_method_attr_idx = get_u2(ptr + 1);
      c->name_and_type_idx = get_u2(ptr + 3);
      *sz = 5;
      return (const_pool_info_t*)c;
    }
    default:
      HB_ERR("Invalid constant tag: %d", *ptr);
      return NULL;
  }

  return NULL;
}


/*
 * KCH NOTE: "The value of the constant_pool_count item is equal
 * to the number of entries in the constant_pool table plus one. A
 * constant_pool index is considered valid if it is greater than zero
 * and less than constant_pool_count, with the exception for
 * constants of type long and double noted in §4.4.5.
 *
 */
static int parse_const_pool(u1* clb, java_class_t* cls, u4* skip) {
  int i;

  cls->const_pool = (const_pool_info_t**)malloc(sizeof(const_pool_info_t*) * (cls->const_pool_count));

  if (!cls->const_pool) {
    HB_ERR("Could not allocate constant pool");
    return -1;
  }

  memset(cls->const_pool, 0, sizeof(const_pool_info_t*) * (cls->const_pool_count));

  for (i = 1; i < cls->const_pool_count; i++) {
    const_pool_info_t* cinfo = NULL;
    u1 sz = 0;

    cinfo = parse_const_pool_entry(clb, &sz);

    if (!cinfo) {
      HB_ERR("Could not parse constant pool entry %d", i);
      return -1;
    }

    cls->const_pool[i] = cinfo;

    clb += sz;
    *skip += sz;

    // KCH: wide entries take up two slots in the const
    // pool for some dumb reason. The second entry
    // "should be valid but is considered unusable"
    if (cls->const_pool[i]->tag == CONSTANT_Long || cls->const_pool[i]->tag == CONSTANT_Double) {
      cls->const_pool[++i] = NULL;
    }
  }

  return 0;
}


static int parse_ixes(u1* clb, java_class_t* cls, u4* skip) {
  int i;

  cls->interfaces = malloc(cls->interfaces_count * sizeof(u2));
  if (!cls->interfaces) {
    HB_ERR("Could not allocate interfaces");
    return -1;
  }
  memset(cls->interfaces, 0, cls->interfaces_count * sizeof(u2));

  for (i = 0; i < cls->interfaces_count; i++) {
    GET_AND_INC(interfaces[i], 2);
    *skip += 2;
  }

  return 0;
}

static inline bool is_const_val_attr(u2 name_idx, java_class_t* cls) {
  return strncmp(hb_get_const_str(name_idx, cls), "ConstantValue", 13) == 0;
}

static inline bool is_lvar_tbl_attr(u2 name_idx, java_class_t* cls) {
  return strncmp(hb_get_const_str(name_idx, cls), "LocalVariableTable", 18) == 0;
}

static inline bool is_line_tbl_attr(u2 name_idx, java_class_t* cls) {
  return strncmp(hb_get_const_str(name_idx, cls), "LineNumberTable", 15) == 0;
}

static inline bool is_source_file_attr(u2 name_idx, java_class_t* cls) {
  return strncmp(hb_get_const_str(name_idx, cls), "SourceFile", 10) == 0;
}

static inline bool is_code_attr(u2 name_idx, java_class_t* cls) { return strncmp(hb_get_const_str(name_idx, cls), "Code", 4) == 0; }


static inline bool is_excp_attr(u2 name_idx, java_class_t* cls) { return strncmp(hb_get_const_str(name_idx, cls), "Exception", 9) == 0; }

static int parse_const_val_attr(u1* clb, field_info_t* fi, java_class_t* cls) {
  // u2 attr_name_idx = get_u2(clb);
  clb += 2;
  // u4 attr_len      = get_u4(clb);
  clb += 4;
  u2 const_val_idx = get_u2(clb);
  clb += 2;

  fi->has_const = 1;
  fi->cpe = cls->const_pool[const_val_idx];

  return 0;
}

static int parse_fields(u1* clb, java_class_t* cls, u4* skip) {
  int i;

  cls->fields = malloc(sizeof(field_info_t) * cls->fields_count);
  if (!cls->fields) {
    HB_ERR("Could not allocate fields");
    return -1;
  }
  memset(cls->fields, 0, sizeof(field_info_t) * cls->fields_count);

  for (i = 0; i < cls->fields_count; i++) {
    GET_AND_INC(fields[i].acc_flags, 2);
    GET_AND_INC(fields[i].name_idx, 2);
    GET_AND_INC(fields[i].desc_idx, 2);
    GET_AND_INC(fields[i].attr_count, 2);

    cls->fields[i].owner = cls;


    *skip += 8;

    if (cls->fields[i].attr_count > 0) {
      int j;
      for (j = 0; j < cls->fields[i].attr_count; j++) {
        u2 name_idx;
        u4 len;

        name_idx = get_u2(clb);
        clb += 2;
        len = get_u4(clb);

        if (is_const_val_attr(name_idx, cls)) {
          parse_const_val_attr(clb - 2, &cls->fields[i], cls);
        }
        clb += 4 + len;
        *skip += 6 + len;
      }
    }

    resolve_field_strings(&cls->fields[i], cls);
  }

  return 0;
}


static int parse_method_code(u1* clb, method_info_t* m, java_class_t* cls) {
  int i;

  u2 attr_name_idx = get_u2(clb);
  clb += 2;
  u4 attr_len = get_u4(clb);
  clb += 4;

  m->code_attr = malloc(sizeof(code_attr_t));
  if (!m->code_attr) {
    HB_ERR("Could not allocate code attribute");
    return -1;
  }
  memset(m->code_attr, 0, sizeof(code_attr_t));

  m->code_attr->attr_name_idx = attr_name_idx;
  m->code_attr->attr_len = attr_len;
  m->code_attr->max_stack = get_u2(clb);
  clb += 2;
  m->code_attr->max_locals = get_u2(clb);
  clb += 2;
  m->code_attr->code_len = get_u4(clb);
  clb += 4;

  m->code_attr->code = malloc(m->code_attr->code_len);
  if (!m->code_attr->code) {
    HB_ERR("Could not allocate code for method!");
    return -1;
  }

  memcpy(m->code_attr->code, clb, m->code_attr->code_len);

  clb += m->code_attr->code_len;

  m->code_attr->excp_table_len = get_u2(clb);
  clb += 2;

  m->code_attr->excp_table = malloc(sizeof(excp_table_t) * m->code_attr->excp_table_len);
  if (!m->code_attr->excp_table) {
    HB_ERR("Could not allocate exception table");
    return -1;
  }

  for (i = 0; i < m->code_attr->excp_table_len; i++) {
    m->code_attr->excp_table[i].start_pc = get_u2(clb);
    clb += 2;
    m->code_attr->excp_table[i].end_pc = get_u2(clb);
    clb += 2;
    m->code_attr->excp_table[i].handler_pc = get_u2(clb);
    clb += 2;
    m->code_attr->excp_table[i].catch_type = get_u2(clb);
    clb += 2;
  }

  m->code_attr->attr_count = get_u2(clb);
  clb += 2;

  m->code_attr->attrs = malloc(sizeof(attr_info_t*) * m->code_attr->attr_count);

  if (!m->code_attr->attrs) {
    HB_ERR("Could not allocate code attributes");
    return -1;
  }
  memset(m->code_attr->attrs, 0, sizeof(attr_info_t*) * m->code_attr->attr_count);

  for (i = 0; i < m->code_attr->attr_count; i++) {
    u2 name_idx = get_u2(clb);
    clb += 2;
    u4 len = get_u4(clb);
    clb += 4;

    m->code_attr->attrs[i] = malloc(6 + len * sizeof(struct line_tbl_entry));

    if (!m->code_attr->attrs[i]) {
      HB_ERR("Could not allocate code attr");
      return -1;
    }
    memset(m->code_attr->attrs[i], 0, 6 + len * sizeof(struct line_tbl_entry));

    attr_info_t* attr = m->code_attr->attrs[i];

    attr->name_idx = name_idx;
    attr->len = len;

    if (is_line_tbl_attr(name_idx, cls)) {
      m->line_info = attr;
      u2 nents = get_u2(clb);
      clb += 2;
      attr->line_num_tbl.line_tbl_len = nents;

      for (int j = 0; j < nents; j++) {
        u2 start_pc = get_u2(clb);
        clb += 2;
        u2 line_num = get_u2(clb);
        clb += 2;
        attr->line_num_tbl.entries[j].start_pc = start_pc;
        attr->line_num_tbl.entries[j].line_num = line_num;
      }
    } else if (is_lvar_tbl_attr(name_idx, cls)) {
      m->lvar_info = attr;
      u2 nents = get_u2(clb);
      clb += 2;
      attr->local_var_tbl.local_var_tbl_len = nents;

      for (int j = 0; j < nents; j++) {
        u2 start_pc = get_u2(clb);
        clb += 2;
        u2 len = get_u2(clb);
        clb += 2;
        u2 nidx = get_u2(clb);
        clb += 2;
        u2 didx = get_u2(clb);
        clb += 2;
        u2 idx = get_u2(clb);
        clb += 2;
        attr->local_var_tbl.entries[j].start_pc = start_pc;
        attr->local_var_tbl.entries[j].len = len;
        attr->local_var_tbl.entries[j].name_idx = nidx;
        attr->local_var_tbl.entries[j].desc_idx = didx;
        attr->local_var_tbl.entries[j].idx = idx;
      }

    } else {
      clb += len;
    }
  }

  return 0;
}


static int parse_methods(u1* clb, java_class_t* cls, u4* skip) {
  int i, j;

  cls->methods = malloc(sizeof(method_info_t) * cls->methods_count);
  if (!cls->methods) {
    HB_ERR("Could not allocate methods");
    return -1;
  }
  memset(cls->methods, 0, sizeof(method_info_t) * cls->methods_count);

  for (i = 0; i < cls->methods_count; i++) {
    GET_AND_INC(methods[i].acc_flags, 2);
    GET_AND_INC(methods[i].name_idx, 2);
    GET_AND_INC(methods[i].desc_idx, 2);
    GET_AND_INC(methods[i].attr_count, 2);

    cls->methods[i].owner = cls;

    *skip += 8;

    if (cls->methods[i].attr_count > 0) {
      for (j = 0; j < cls->methods[i].attr_count; j++) {
        u2 name_idx;
        u4 len;

        name_idx = get_u2(clb);
        clb += 2;
        len = get_u4(clb);
        clb += 4;
        *skip += 6;

        if (is_code_attr(name_idx, cls)) {
          parse_method_code(clb - 6, &cls->methods[i], cls);
        }

        clb += len;
        *skip += len;
      }
    }

    resolve_method_strings(&cls->methods[i], cls);
  }

  return 0;
}


static int parse_attr(u1* clb, java_class_t* cls, u4 idx, u4* skip) {
  attr_info_t* ainfo = NULL;
  u2 name_idx = get_u2(clb);
  clb += 2;
  u4 len = get_u4(clb);
  clb += 4;

  *skip += 6;

  cls->attrs[idx] = malloc(6 + len);
  if (!cls->attrs[idx]) {
    HB_ERR("Could not allocate attribute");
    return -1;
  }
  memset(cls->attrs[idx], 0, 6 + len);

  ainfo = cls->attrs[idx];

  ainfo->name_idx = name_idx;
  ainfo->len = len;

  if (is_source_file_attr(name_idx, cls)) {
    ainfo->src_file.srcfile_idx = get_u2(clb);
    clb += 2;
    const char* srcfile = hb_get_const_str(ainfo->src_file.srcfile_idx, cls);
    cls->src_file = srcfile;
  }

  *skip += len;

  return 0;
}


static int parse_attrs(u1* clb, java_class_t* cls, u4* skip) {
  cls->attrs = malloc(sizeof(attr_info_t*) * cls->attr_count);
  if (!cls->attrs) {
    HB_ERR("Could not allocate attributes");
    return -1;
  }
  memset(cls->attrs, 0, sizeof(attr_info_t*) * cls->attr_count);

  for (int i = 0; i < cls->attr_count; i++) {
    parse_attr(clb, cls, i, skip);
  }

  return 0;
}


static int parse_class(u1* clb, java_class_t* cls) {
  u4 skip;

  GET_AND_INC(magic, 4);
  GET_AND_INC(minor_version, 2);
  GET_AND_INC(major_version, 2);
  GET_AND_INC(const_pool_count, 2);

  PARSE_AND_CHECK(const_pool_count, parse_const_pool, "const pool");

  GET_AND_INC(acc_flags, 2);
  GET_AND_INC(this, 2);
  GET_AND_INC(super, 2);
  GET_AND_INC(interfaces_count, 2);

  PARSE_AND_CHECK(interfaces_count, parse_ixes, "interfaces");

  GET_AND_INC(fields_count, 2);

  PARSE_AND_CHECK(fields_count, parse_fields, "fields");

  GET_AND_INC(methods_count, 2);

  PARSE_AND_CHECK(methods_count, parse_methods, "methods");

  GET_AND_INC(attr_count, 2);

  PARSE_AND_CHECK(attr_count, parse_attrs, "attributes");

  return 0;
}


/* TODO: deeper verification required */
static int verify_class(java_class_t* cls) {
  if (cls->magic != JAVA_MAGIC) {
    HB_ERR("Bad magic: 0x%04x", cls->magic);
    return -1;
  }

  return 0;
}

static void print_class_info(java_class_t* cls) {
  HB_INFO("Minor Version: %x", cls->minor_version);
  HB_INFO("Major Verison: %x", cls->major_version);
  HB_INFO("Const pool count: %d", cls->const_pool_count);
  HB_INFO("Interfaces count: %d", cls->interfaces_count);
  HB_INFO("Fields count: %d", cls->fields_count);
  HB_INFO("Methods count: %d", cls->methods_count);
  HB_INFO("Attributes count: %d", cls->attr_count);
}


#define MAX_LINE_COUNT 8092
int hb_read_source_file(java_class_t* cls) {
  int n = 0;
  size_t bytes = 0;

  if (cls->src_lines) return 0;

  FILE* f = hb_open(cls->src_file, "r");
  if (!f) {
    HB_ERR("Could not open source file: %s", cls->src_file);
    return -1;
  }

  cls->src_lines = malloc(MAX_LINE_COUNT * sizeof(char*));
  memset(cls->src_lines, 0, MAX_LINE_COUNT * sizeof(char*));

  while (getline(&cls->src_lines[n], &bytes, f) != -1) {
    n++;
  }

  cls->num_src_lines = n;

  fclose(f);

  cls->src_lines = realloc(cls->src_lines, n * sizeof(char*));
  if (!cls->src_lines) {
    HB_ERR("Realloc failed");
    return -1;
  }

  return 0;
}

/*
 * KCH TODO: should throw a ClassNotFoundException on error,
 * though there's some chicken-and-egg going on here
 */
java_class_t* hb_load_class(const char* path) {
  java_class_t* cls = NULL;
  u1* class_bytes = NULL;

  class_bytes = open_class_file(path);

  if (!class_bytes) {
    HB_ERR("Could not load class file");
    return NULL;
  }

  cls = malloc(sizeof(java_class_t));
  if (!cls) {
    HB_ERR("Could not allocate class struct");
    return NULL;
  }
  memset(cls, 0, sizeof(java_class_t));

  if (parse_class(class_bytes, cls) != 0) {
    HB_ERR("Could not parse class file");
    return NULL;
  }

  if (verify_class(cls) != 0) {
    HB_ERR("Could not verify class");
    return NULL;
  }

  cls->name = (strchr(path, '.') ? strtok((char*)path, ".") : path);

  CL_DEBUG("Class file (for class %s) verified and loaded", hb_get_class_name(cls));

  cls->field_vals = malloc(sizeof(var_t) * cls->fields_count);
  if (!cls->field_vals) {
    HB_ERR("Could not allocate class fields");
    return NULL;
  }
  memset(cls->field_vals, 0, sizeof(var_t) * cls->fields_count);

  cls->status = CLS_LOADED;

  return cls;
}
