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

#define CONSTANT_Class	7
#define CONSTANT_Fieldref	9
#define CONSTANT_Methodref	10
#define CONSTANT_InterfaceMethodref	11
#define CONSTANT_String	8
#define CONSTANT_Integer	3
#define CONSTANT_Float	4
#define CONSTANT_Long	5
#define CONSTANT_Double	         6
#define CONSTANT_NameAndType	12
#define CONSTANT_Utf8	         1
#define CONSTANT_MethodHandle	15
#define CONSTANT_MethodType	16
#define CONSTANT_InvokeDynamic	18

#include <types.h>

/* BEGIN CONSTANT TYPES */
typedef struct CONSTANT_Class_info {
	u1 tag;
	u2 name_idx;
} CONSTANT_Class_info_t;

typedef struct CONSTANT_Fieldref_info {
	u1 tag;
	u2 class_idx;
	u2 name_and_type_idx;
} CONSTANT_Fieldref_info_t;

typedef struct CONSTANT_Methodref_info {
	u1 tag;
	u2 class_idx;
	u2 name_and_type_idx;
} CONSTANT_Methodref_info_t;

typedef struct CONSTANT_InterfaceMethodref_info {
	u1 tag;
	u2 class_idx;
	u2 name_and_type_idx;
} CONSTANT_InterfaceMethodref_info_t;

typedef struct CONSTANT_String_info {
	u1 tag;
	u2 str_idx;
} CONSTANT_String_info_t;

typedef struct CONSTANT_Integer_info {
	u1 tag;
	u4 bytes;
} CONSTANT_Integer_info_t;

typedef struct CONSTANT_Float_info {
	u1 tag;
	u4 bytes;
} CONSTANT_Float_info_t;

typedef struct CONSTANT_Long_info {
	u1 tag;
	u4 hi_bytes;
	u4 lo_bytes;
} CONSTANT_Long_info_t;

typedef struct CONSTANT_Double_info {
	u1 tag;
	u4 hi_bytes;
	u4 lo_bytes;
} CONSTANT_Double_info_t;

typedef struct CONSTANT_NameAndType_info {
	u1 tag;
	u2 name_idx;
	u2 desc_idx;
} CONSTANT_NameAndType_info_t;

typedef struct CONSTANT_Utf8_info {
	u1 tag;
	u2 len;
	u1 bytes[0];
} CONSTANT_Utf8_info_t;

typedef struct CONSTANT_MethodHandle_info {
	u1 tag;
	u1 ref_kind;
	u2 ref_idx;
} CONSTANT_MethodHandle_info_t;

typedef struct CONSTANT_MethodType_info {
	u1 tag;
	u2 desc_idx;
} CONSTANT_MethodType_info_t;

typedef struct CONSTANT_InvokeDynamic_info {
	u1 tag;
	u2 bootstrap_method_attr_idx;
	u2 name_and_type_idx;
} CONSTANT_InvokeDynamic_info_t;
