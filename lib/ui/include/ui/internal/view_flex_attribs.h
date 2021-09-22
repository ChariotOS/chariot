FLEX_ATTRIBUTE(width, float, NAN)
FLEX_ATTRIBUTE(height, float, NAN)

FLEX_ATTRIBUTE(left, float, NAN)
FLEX_ATTRIBUTE(right, float, NAN)
FLEX_ATTRIBUTE(top, float, NAN)
FLEX_ATTRIBUTE(bottom, float, NAN)


FLEX_ATTRIBUTE(padding, ui::edges, {})
FLEX_ATTRIBUTE(margin, ui::edges, {})

FLEX_ATTRIBUTE(justify_content, ui::FlexAlign, ui::FlexAlign::Start)
FLEX_ATTRIBUTE(align_content, ui::FlexAlign, ui::FlexAlign::Stretch)
FLEX_ATTRIBUTE(align_items, ui::FlexAlign, ui::FlexAlign::Stretch)
FLEX_ATTRIBUTE(align_self, ui::FlexAlign, ui::FlexAlign::Auto)

FLEX_ATTRIBUTE(position, ui::FlexPosition, ui::FlexPosition::Relative)
FLEX_ATTRIBUTE(direction, ui::FlexDirection, ui::FlexDirection::Column)
FLEX_ATTRIBUTE(wrap, ui::FlexWrap, ui::FlexWrap::NoWrap)

FLEX_ATTRIBUTE(grow, float, 0.0)
FLEX_ATTRIBUTE(shrink, float, 1.0)
FLEX_ATTRIBUTE(order, int, 0)
FLEX_ATTRIBUTE(basis, float, NAN)

#undef FLEX_ATTRIBUTE
