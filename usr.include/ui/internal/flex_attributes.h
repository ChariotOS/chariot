// Following are the list of properties associated with an item.
//
// Each property is exposed with getter and setter functions, for instance
// the `width' property can be get and set using the respective
// `flex_item_get_width()' and `flex_item_set_width()' functions.
//
// You can also see the type and default value for each property.

FLEX_ATTRIBUTE(width, float, NAN)
FLEX_ATTRIBUTE(height, float, NAN)

FLEX_ATTRIBUTE(left, float, NAN)
FLEX_ATTRIBUTE(right, float, NAN)
FLEX_ATTRIBUTE(top, float, NAN)
FLEX_ATTRIBUTE(bottom, float, NAN)

FLEX_ATTRIBUTE(padding_left, float, 0)
FLEX_ATTRIBUTE(padding_right, float, 0)
FLEX_ATTRIBUTE(padding_top, float, 0)
FLEX_ATTRIBUTE(padding_bottom, float, 0)

FLEX_ATTRIBUTE(margin_left, float, 0)
FLEX_ATTRIBUTE(margin_right, float, 0)
FLEX_ATTRIBUTE(margin_top, float, 0)
FLEX_ATTRIBUTE(margin_bottom, float, 0)

FLEX_ATTRIBUTE(justify_content, flex_align, FLEX_ALIGN_START)
FLEX_ATTRIBUTE(align_content, flex_align, FLEX_ALIGN_STRETCH)
FLEX_ATTRIBUTE(align_items, flex_align, FLEX_ALIGN_STRETCH)
FLEX_ATTRIBUTE(align_self, flex_align, FLEX_ALIGN_AUTO)

FLEX_ATTRIBUTE(position, flex_position, FLEX_POSITION_RELATIVE)
FLEX_ATTRIBUTE(direction, flex_direction, FLEX_DIRECTION_COLUMN)
FLEX_ATTRIBUTE(wrap, flex_wrap, FLEX_WRAP_NO_WRAP)

FLEX_ATTRIBUTE(grow, float, 0.0)
FLEX_ATTRIBUTE(shrink, float, 1.0)
FLEX_ATTRIBUTE(order, int, 0)
FLEX_ATTRIBUTE(basis, float, NAN)

