// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See the LICENSE.txt file in the project root
// for the license information.

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __FLEX_H_
#define __FLEX_H_


struct flex_item;

// Create a new flex item.
struct flex_item *flex_item_new(void);

// Free memory associated with a flex item and its children.
// This function can only be called on a root item.
void flex_item_free(struct flex_item *item);

// Manage items.
void flex_item_add(struct flex_item *item, struct flex_item *child);
void flex_item_insert(struct flex_item *item, unsigned int index, struct flex_item *child);
struct flex_item *flex_item_delete(struct flex_item *item, unsigned int index);
unsigned int flex_item_count(struct flex_item *item);
struct flex_item *flex_item_child(struct flex_item *item, unsigned int index);
struct flex_item *flex_item_parent(struct flex_item *item);
struct flex_item *flex_item_root(struct flex_item *item);

// Layout the items associated with this item, as well as their children.
// This function can only be called on a root item whose `width' and `height'
// properties have been set.
void flex_layout(struct flex_item *item);

// Retrieve the layout frame associated with an item. These functions should
// be called *after* the layout is done.
float flex_item_get_frame_x(struct flex_item *item);
float flex_item_get_frame_y(struct flex_item *item);
float flex_item_get_frame_width(struct flex_item *item);
float flex_item_get_frame_height(struct flex_item *item);

typedef enum {
  FLEX_ALIGN_AUTO = 0,
  FLEX_ALIGN_STRETCH,
  FLEX_ALIGN_CENTER,
  FLEX_ALIGN_START,
  FLEX_ALIGN_END,
  FLEX_ALIGN_SPACE_BETWEEN,
  FLEX_ALIGN_SPACE_AROUND,
  FLEX_ALIGN_SPACE_EVENLY
} flex_align;

typedef enum { FLEX_POSITION_RELATIVE = 0, FLEX_POSITION_ABSOLUTE } flex_position;

typedef enum {
  FLEX_DIRECTION_ROW = 0,
  FLEX_DIRECTION_ROW_REVERSE,
  FLEX_DIRECTION_COLUMN,
  FLEX_DIRECTION_COLUMN_REVERSE
} flex_direction;

typedef enum { FLEX_WRAP_NO_WRAP = 0, FLEX_WRAP_WRAP, FLEX_WRAP_WRAP_REVERSE } flex_wrap;

// size[0] == width, size[1] == height
typedef void (*flex_self_sizing)(struct flex_item *item, float size[2]);
typedef void (*flex_pre_layout)(struct flex_item *item);

#ifndef FLEX_ATTRIBUTE
#define FLEX_ATTRIBUTE(name, type, def)              \
  type flex_item_get_##name(struct flex_item *item); \
  void flex_item_set_##name(struct flex_item *item, type value);
#endif

#else  // !__FLEX_H_

#ifndef FLEX_ATTRIBUTE
#define FLEX_ATTRIBUTE(name, type, def)
#endif

#endif

#include <ui/internal/flex_attributes.h>
// An item can store an arbitrary pointer, which can be used by bindings as
// the address of a managed object.
FLEX_ATTRIBUTE(managed_ptr, void *, NULL)

// An item can provide a self_sizing callback function that will be called
// during layout and which can customize the dimensions (width and height)
// of the item.
FLEX_ATTRIBUTE(self_sizing, flex_self_sizing, NULL)
FLEX_ATTRIBUTE(pre_layout, flex_pre_layout, NULL)

#undef FLEX_ATTRIBUTE


#ifdef __cplusplus
}
#endif
