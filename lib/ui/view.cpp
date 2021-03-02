#include <ui/view.h>
#include <ui/window.h>




ui::view::view() {
  memset(m_frame, 0, sizeof(m_frame));

  set_flex_direction(ui::FlexDirection::Column);
  // set_flex_grow(1.0);

  set_flex_align_items(ui::FlexAlign::Stretch);
  set_flex_align_content(ui::FlexAlign::Stretch);
}

ui::view::~view(void) {
  /*
for (int i = flex_item_count(m_fi) - 1; i >= 0; i--) {
flex_item_delete(m_fi, i);
}
// just delete the flex item
flex_item_free(m_fi);
  */

  auto *srf = surface();
  if (srf) {
    if (srf->hovered == this) srf->hovered = NULL;
    if (srf->focused == this) srf->focused = NULL;
  }
  m_children.clear();
}

void ui::view::clear(void) { m_children.clear(); }



void ui::view::repaint(bool do_invalidate) {
  // if we aren't mounted, don't paint - it doesn't make sense
  if (surface() == NULL) return;


  auto s = get_scribe();

  // draw the border
  gfx::rect border = gfx::rect(0, 0, width(), height());

  if (background) {
    s.fill_rect(border, get_background());
  }

  for (int i = 0; i < bordersize; i++) {
    s.draw_rect(border, bordercolor);
    border.x += 1;
    border.y += 1;
    border.w -= 2;
    border.h -= 2;
  }

  paint_event();
  each_child([](auto &c) { c.repaint(false); });
  if (do_invalidate) invalidate();
}


void ui::view::set_focused(void) {
  auto *win = surface();
  if (win != NULL) {
    if (win->focused) {
      win->focused->on_blur();
    }
    win->focused = this;
    on_focused();
  }
}


void ui::view::dispatch_mouse_event(ui::mouse_event &event) {
  // copy the event and adjust it to the local bounds
  ui::mouse_event ev = event;
  ev.x -= left();
  ev.y -= top();

  // what we do here is based on what kind of event we have.
  bool sent_to_child = false;
  each_child([&](auto &child) {
    if (sent_to_child) return;
    // if the mouse event is within the child, send it there instead
    auto crel = child.relative();
    if (crel.contains(ev.x, ev.y)) {
      child.dispatch_mouse_event(ev);
      sent_to_child = true;
    }
  });

  if (!sent_to_child) {
		this->mouse_event(ev);
  }
}


bool ui::view::event(ui::event &ev) {
  auto rel = relative();
  // try to handle each type
  switch (ev.get_type()) {
    case ui::event::type::mouse: {
      // translate the mouse event to maybe within this view
      ui::mouse_event mv = ev.as<ui::mouse_event>();
      if (rel.contains(mv.x, mv.y)) {
        dispatch_mouse_event(mv);
        return true;
      }
      break;
    }

    case ui::event::type::keyup: {
      auto mv = ev.as<ui::keyup_event>();
      on_keyup(mv);
      break;
    }

    case ui::event::type::keydown: {
      auto mv = ev.as<ui::keydown_event>();
      on_keydown(mv);
      break;
    }
  }


  // walk through the children and try to run the event through them
  for (auto &v : m_children) {
    if (v->event(ev)) return true;
  }

  return false;  // we didn't absorb this
}


gfx::scribe ui::view::get_scribe(void) {
  gfx::scribe s(*surface()->bmp());

  s.enter();
  auto r = absolute_rect();

  s.state().clip = r;
  s.state().offset = gfx::point(r.x, r.y);

  return s;
}


// invalidate the whole view
void ui::view::invalidate(void) { surface()->invalidate(absolute_rect()); }




void ui::view::do_reflow(void) {
  // always just ask the window to reflow.
  surface()->reflow();
}




void ui::view::set_size(ui::Direction dir, float sz) {
  switch (dir) {
    case ui::Direction::Vertical:
      m_height = sz;
      break;
    case ui::Direction::Horizontal:
      m_width = sz;
      break;
  }
  return;
}


void ui::view::set_size(float w, float h) {
  m_width = w;
  m_height = h;
}


gfx::rect ui::view::absolute_rect(void) {
  gfx::rect r = relative();
  if (m_parent != NULL) {
    gfx::rect p = parent()->absolute_rect();
    r.x += p.x;
    r.y += p.y;
    return r;
  }
  return r;
}

gfx::rect ui::view::padded_area(void) {
  gfx::rect r;
  r.x = get_flex_padding().left;
  r.y = get_flex_padding().top;
  r.w = width() - r.x - get_flex_padding().right;
  r.h = height() - r.y - get_flex_padding().bottom;

  r.shrink(bordersize);
  return r;
}


static void recurse_mount(ui::view *v) {
  v->mounted();
  v->each_child([](ui::view &other) { recurse_mount(&other); });
};


void ui::view::add(ui::view *v) {
  v->m_parent = this;
  m_children.push(ck::unique_ptr(v));
  // add the child's flexbox on the end
  // flex_item_add(m_fi, v->m_fi);
  if (surface()) {
    do_reflow();
    recurse_mount(v);
  }
}




/////////////////////////////////////////////////////////////////////////////////


#define LAYOUT_LOG(fmt, args...) \
  if (this->log_layouts) {       \
    printf(fmt, ##args);         \
  }

#define _LAYOUT_FRAME(child, name) child->m_frame[layout.frame_##name]

#define CHILD_MAIN_POS(child) _LAYOUT_FRAME(child, main_pos)
#define CHILD_CROSS_POS(child) _LAYOUT_FRAME(child, cross_pos)
#define CHILD_MAIN_SIZE(child) _LAYOUT_FRAME(child, main_size)
#define CHILD_CROSS_SIZE(child) _LAYOUT_FRAME(child, cross_size)

#define CHILD_MARGIN(child, if_vertical, if_horizontal) \
  (layout.vertical ? child->m_margin.if_vertical : child->m_margin.if_horizontal)


ui::flex_layout::flex_layout(void) {}

ui::view *ui::flex_layout::child_at(ui::view *v, int i) {
  return v->m_children[(ordered_indices != NULL ? ordered_indices[i] : i)].get();
}



void ui::flex_layout::init(ui::view &v, float width, float height) {
  assert(v.m_padding.left >= 0);
  assert(v.m_padding.right >= 0);
  assert(v.m_padding.top >= 0);
  assert(v.m_padding.bottom >= 0);

  width -= v.m_padding.left + v.m_padding.right;
  height -= v.m_padding.top + v.m_padding.bottom;


  if (width < 0) width = 0;
  if (height < 0) height = 0;
  assert(width >= 0);
  assert(height >= 0);

  reverse_main = false;
  vertical = true;

  switch (v.m_direction) {
    case ui::FlexDirection::RowReverse:
      reverse_main = true;
    case ui::FlexDirection::Row:
      vertical = false;
      size_dim = width;
      align_dim = height;
      frame_main_pos = 0;
      frame_cross_pos = 1;
      frame_main_size = 2;
      frame_cross_size = 3;
      break;

    case ui::FlexDirection::ColumnReverse:
      reverse_main = true;
    case ui::FlexDirection::Column:
      size_dim = height;
      align_dim = width;
      frame_main_pos = 1;
      frame_cross_pos = 0;
      frame_main_size = 3;
      frame_cross_size = 2;
      break;

    default:
      assert(false && "incorrect direction");
  }

  ordered_indices = NULL;
  if (v.m_should_order_children && v.m_children.size() > 0) {
    unsigned int *indices = new unsigned int[v.m_children.size()];
    assert(indices != NULL);

    // Creating a list of item indices sorted using the children's `order'
    // attribute values. We are using a simple insertion sort as we need
    // stability (insertion order must be preserved) and cross-platform
    // support. We should eventually switch to merge sort (or something
    // else) if the number of items becomes significant enough.
    for (unsigned int i = 0; i < v.m_children.size(); i++) {
      indices[i] = i;
      for (unsigned int j = i; j > 0; j--) {
        unsigned int prev = indices[j - 1];
        unsigned int curr = indices[j];
        if (v.m_children[prev]->m_order <= v.m_children[curr]->m_order) {
          break;
        }
        indices[j - 1] = curr;
        indices[j] = prev;
      }
    }
    ordered_indices = indices;
  }


  flex_dim = 0;
  flex_grows = 0;
  flex_shrinks = 0;

  reverse_cross = false;
  wrap = v.m_wrap != ui::FlexWrap::NoWrap;
  if (wrap) {
    if (v.m_wrap == ui::FlexWrap::Reverse) {
      reverse_cross = true;
      cross_pos = align_dim;
    }
  } else {
    cross_pos = vertical ? v.m_padding.left : v.m_padding.top;
  }

  need_lines = wrap;  // && item->align_content != FLEX_ALIGN_START;
  lines.clear();
  lines_sizes = 0;
}


void ui::flex_layout::reset(void) {
  line_dim = wrap ? 0 : align_dim;
  flex_dim = size_dim;
  extra_flex_dim = 0;
  flex_grows = 0;
  flex_shrinks = 0;
}



ui::flex_layout::~flex_layout(void) {
  if (ordered_indices) delete[] ordered_indices;
}


static bool layout_align(ui::FlexAlign align, float flex_dim, unsigned int children_count, float *pos_p,
                         float *spacing_p, bool stretch_allowed) {
  assert(flex_dim > 0);

  float pos = 0;
  float spacing = 0;
  switch (align) {
    case ui::FlexAlign::Start:
      break;

    case ui::FlexAlign::End:
      pos = flex_dim;
      break;

    case ui::FlexAlign::Center:
      pos = flex_dim / 2;
      break;

    case ui::FlexAlign::SpaceBetween:
      if (children_count > 0) {
        spacing = flex_dim / (children_count - 1);
      }
      break;

    case ui::FlexAlign::SpaceAround:
      if (children_count > 0) {
        spacing = flex_dim / children_count;
        pos = spacing / 2;
      }
      break;

    case ui::FlexAlign::SpaceEvenly:
      if (children_count > 0) {
        spacing = flex_dim / (children_count + 1);
        pos = spacing;
      }
      break;

    case ui::FlexAlign::Stretch:
      if (stretch_allowed) {
        spacing = flex_dim / children_count;
        break;
      }
      // fall through

    default:
      return false;
  }

  *pos_p = pos;
  *spacing_p = spacing;
  return true;
}

bool ui::view::layout(void) {
  if (parent() != NULL) return false;
  // if (isnan(m_width)) return false;
  // if (isnan(m_height)) return false;

  layout(m_width, m_height);

  /* fix up the width and height for the root node */
  m_frame[2] = m_width;
  m_frame[3] = m_height;

  return true;
}


void ui::view::layout(float width, float height) {
  /* if we have no children, there is no reason to layout */
  if (m_children.size() == 0) return;


  flex_layout layout;
  layout.init(*this, width, height);

  layout.reset();

  unsigned int last_layout_child = 0;
  unsigned int relative_children_count = 0;
  for (unsigned int i = 0; i < m_children.size(); i++) {
    auto *child = layout.child_at(this, i);

    // Items with an absolute position have their frames determined
    // directly and are skipped during layout.
    if (child->m_position == ui::FlexPosition::Absolute) {
#define ABSOLUTE_SIZE(val, pos1, pos2, dim) (!isnan(val) ? val : (!isnan(pos1) && !isnan(pos2) ? dim - pos2 - pos1 : 0))

#define ABSOLUTE_POS(pos1, pos2, size, dim) (!isnan(pos1) ? pos1 : (!isnan(pos2) ? dim - size - pos2 : 0))

      float child_width = ABSOLUTE_SIZE(child->m_width, child->m_left, child->m_right, width);

      float child_height = ABSOLUTE_SIZE(child->m_height, child->m_top, child->m_bottom, height);

      float child_x = ABSOLUTE_POS(child->m_left, child->m_right, child_width, width);

      float child_y = ABSOLUTE_POS(child->m_top, child->m_bottom, child_height, height);

      child->m_frame[0] = child_x;
      child->m_frame[1] = child_y;
      child->m_frame[2] = child_width;
      child->m_frame[3] = child_height;

      // Now that the item has a frame, we can layout its children.
      child->layout(child->m_frame[2], child->m_frame[3]);

#undef ABSOLUTE_POS
#undef ABSOLUTE_SIZE
      continue;
    }

    // Initialize frame. The sizes here are just recommendations
    child->m_frame[0] = 0;
    child->m_frame[1] = 0;
    child->m_frame[2] = child->m_width;
    child->m_frame[3] = child->m_height;

    // Main axis size defaults to 0.
    if (isnan(CHILD_MAIN_SIZE(child))) {
      CHILD_MAIN_SIZE(child) = 0;
    }

    // Cross axis size defaults to the parent's size (or line size in wrap
    // mode, which is calculated later on).
    if (isnan(CHILD_CROSS_SIZE(child))) {
      if (layout.wrap) {
        layout.need_lines = true;
      } else {
        CHILD_CROSS_SIZE(child) =
            (layout.vertical ? width : height) - CHILD_MARGIN(child, left, top) - CHILD_MARGIN(child, right, bottom);
      }
    }

    // Call the self_sizing callback if provided. Only non-NAN values
    // are taken into account. If the item's cross-axis align property
    // is set to stretch, ignore the value returned by the callback.
    if (child->m_parent != NULL) {
      float size[2] = {child->m_frame[2], child->m_frame[3]};

      child->flex_self_sizing(size[0], size[1]);

      for (unsigned int j = 0; j < 2; j++) {
        unsigned int size_off = j + 2;
        if (size_off == layout.frame_cross_size && child->child_align() == ui::FlexAlign::Stretch) {
          continue;
        }
        float val = size[j];
        if (!isnan(val)) {
          child->m_frame[size_off] = val;
        }
      }
    }

    // Honor the `basis' property which overrides the main-axis size.
    if (!isnan(child->m_basis)) {
      assert(child->m_basis >= 0);
      CHILD_MAIN_SIZE(child) = child->m_basis;
    }

    float child_size = CHILD_MAIN_SIZE(child);
    if (layout.wrap) {
      if (layout.flex_dim < child_size) {
        // Not enough space for this child on this line, layout the
        // remaining items and move it to a new line.
        this->layout_items(last_layout_child, i, relative_children_count, layout);

        layout.reset();

        last_layout_child = i;
        relative_children_count = 0;
      }

      float child_size2 = CHILD_CROSS_SIZE(child);
      if (!isnan(child_size2) && child_size2 > layout.line_dim) {
        layout.line_dim = child_size2;
      }
    }

    assert(child->m_grow >= 0);
    assert(child->m_shrink >= 0);

    layout.flex_grows += child->m_grow;
    layout.flex_shrinks += child->m_shrink;

    layout.flex_dim -= child_size + (CHILD_MARGIN(child, top, left) + CHILD_MARGIN(child, bottom, right));

    relative_children_count++;

    if (child_size > 0 && child->m_grow > 0) {
      layout.extra_flex_dim += child_size;
    }
  }

  // Layout remaining items in wrap mode, or everything otherwise.
  layout_items(last_layout_child, m_children.size(), relative_children_count, layout);

  if (isnan(height)) {
    printf("height is nan!\n");
  }

  if (isnan(width)) {
    printf("width is nan!\n");
  }


  // In wrap mode we may need to tweak the position of each line according to
  // the align_content property as well as the cross-axis size of items that
  // haven't been set yet.
  if (layout.need_lines && layout.lines.size() > 0) {
    float pos = 0;
    float spacing = 0;
    float flex_dim = layout.align_dim - layout.lines_sizes;
    if (flex_dim > 0) {
      assert(layout_align(m_align_content, flex_dim, layout.lines.size(), &pos, &spacing, true) &&
             "incorrect align_content");
    }

    float old_pos = 0;
    if (layout.reverse_cross) {
      pos = layout.align_dim - pos;
      old_pos = layout.align_dim;
    }

    for (unsigned int i = 0; i < layout.lines.size(); i++) {
      auto *line = &layout.lines[i];

      if (layout.reverse_cross) {
        pos -= line->size;
        pos -= spacing;
        old_pos -= line->size;
      }

      // Re-position the children of this line, honoring any child
      // alignment previously set within the line.
      for (unsigned int j = line->child_begin; j < line->child_end; j++) {
        auto *child = layout.child_at(this, j);
        if (child->m_position == ui::FlexPosition::Absolute) {
          // Should not be re-positioned.
          continue;
        }
        if (isnan(CHILD_CROSS_SIZE(child))) {
          // If the child's cross axis size hasn't been set it, it
          // defaults to the line size.
          CHILD_CROSS_SIZE(child) = line->size + (m_align_content == ui::FlexAlign::Stretch ? spacing : 0);
        }
        CHILD_CROSS_POS(child) = pos + (CHILD_CROSS_POS(child) - old_pos);
      }

      if (!layout.reverse_cross) {
        pos += line->size;
        pos += spacing;
        old_pos += line->size;
      }
    }
  }
}




void ui::view::layout_items(unsigned int child_begin, unsigned int child_end, unsigned int children_count,
                            struct flex_layout &layout) {
  assert(children_count <= (child_end - child_begin));
  if (children_count <= 0) {
    return;
  }

  if (layout.flex_dim > 0 && layout.extra_flex_dim > 0) {
    // If the container has a positive flexible space, let's add to it
    // the sizes of all flexible children.
    layout.flex_dim += layout.extra_flex_dim;
  }

  // Determine the main axis initial position and optional spacing.
  float pos = 0;
  float spacing = 0;
  if (layout.flex_grows == 0 && layout.flex_dim > 0) {
    assert(layout_align(m_justify_content, layout.flex_dim, children_count, &pos, &spacing, false) &&
           "incorrect justify_content");
    if (layout.reverse_main) {
      pos = layout.size_dim - pos;
    }
  }

  if (layout.reverse_main) {
    pos -= layout.vertical ? m_padding.bottom : m_padding.right;
  } else {
    pos += layout.vertical ? m_padding.top : m_padding.left;
  }
  if (layout.wrap && layout.reverse_cross) {
    layout.cross_pos -= layout.line_dim;
  }

  for (unsigned int i = child_begin; i < child_end; i++) {
    auto *child = layout.child_at(this, i);
    if (child->m_position == ui::FlexPosition::Absolute) {
      // Already positioned.
      continue;
    }

    // Grow or shrink the main axis item size if needed.
    float flex_size = 0;
    if (layout.flex_dim > 0) {
      if (child->m_grow != 0) {
        CHILD_MAIN_SIZE(child) = 0;  // Ignore previous size when growing.
        flex_size = (layout.flex_dim / layout.flex_grows) * child->m_grow;
      }
    } else if (layout.flex_dim < 0) {
      if (child->m_shrink != 0) {
        flex_size = (layout.flex_dim / layout.flex_shrinks) * child->m_shrink;
      }
    }
    CHILD_MAIN_SIZE(child) += flex_size;

    // Set the cross axis position (and stretch the cross axis size if
    // needed).
    float align_size = CHILD_CROSS_SIZE(child);
    float align_pos = layout.cross_pos + 0;
    switch (child->child_align()) {
      case ui::FlexAlign::End:
        align_pos += layout.line_dim - align_size - CHILD_MARGIN(child, right, bottom);
        break;

      case ui::FlexAlign::Center:
        align_pos += (layout.line_dim / 2) - (align_size / 2) +
                     (CHILD_MARGIN(child, left, top) - CHILD_MARGIN(child, right, bottom));
        break;

      case ui::FlexAlign::Stretch:
        if (align_size == 0) {
          CHILD_CROSS_SIZE(child) =
              layout.line_dim - (CHILD_MARGIN(child, left, top) + CHILD_MARGIN(child, right, bottom));
        }
        // fall through

      case ui::FlexAlign::Start:
        align_pos += CHILD_MARGIN(child, left, top);
        break;

      default:
        assert(false && "incorrect align_self");
    }
    CHILD_CROSS_POS(child) = align_pos;

    // Set the main axis position.
    if (layout.reverse_main) {
      pos -= CHILD_MARGIN(child, bottom, right);
      pos -= CHILD_MAIN_SIZE(child);
      CHILD_MAIN_POS(child) = pos;
      pos -= spacing;
      pos -= CHILD_MARGIN(child, top, left);
    } else {
      pos += CHILD_MARGIN(child, top, left);
      CHILD_MAIN_POS(child) = pos;
      pos += CHILD_MAIN_SIZE(child);
      pos += spacing;
      pos += CHILD_MARGIN(child, bottom, right);
    }

    // Now that the item has a frame, we can layout its children.
    child->layout(child->m_frame[2], child->m_frame[3]);
  }

  if (layout.wrap && !layout.reverse_cross) {
    layout.cross_pos += layout.line_dim;
  }

  if (layout.need_lines) {
    layout.lines.push({
        .child_begin = child_begin,
        .child_end = child_end,
        .size = layout.line_dim,
    });
    layout.lines_sizes += layout.line_dim;
  }
}


ui::FlexAlign ui::view::child_align(void) {
  auto align = m_align_self;
  if (align == Auto) {
    align = parent()->m_align_items;
  }
  return align;
}

