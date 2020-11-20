#include <ui/view.h>
#include <ui/window.h>

static void view_self_sizing_callback(struct flex_item *item, float size[2]) {
  ui::view *view = (ui::view *)flex_item_get_managed_ptr(item);

  // printf("%f %f\n", size[0], size[1]);
  view->flex_self_sizing(size[0], size[1]);
}

static void view_pre_layout_callback(struct flex_item *item) {
  float w, h;

  w = flex_item_get_width(item);
  h = flex_item_get_height(item);


  if (!isnan(w)) if (w < 0) flex_item_set_width(item, 0.0);
  if (!isnan(h)) if (h < 0) flex_item_set_height(item, 0.0);
}


ui::view::view() {
  m_fi = flex_item_new();

  flex_item_set_managed_ptr(m_fi, (void *)this);
  flex_item_set_self_sizing(m_fi, view_self_sizing_callback);
  flex_item_set_pre_layout(m_fi, view_pre_layout_callback);
  set_flex_direction(FLEX_DIRECTION_COLUMN);
  // set_flex_grow(1.0);

  set_flex_align_items(FLEX_ALIGN_STRETCH);
  set_flex_align_content(FLEX_ALIGN_STRETCH);
}

ui::view::~view(void) {
  for (int i = flex_item_count(m_fi) - 1; i >= 0; i--) {
    flex_item_delete(m_fi, i);
  }

  // just delete the flex item
  flex_item_free(m_fi);
  auto *win = window();
  if (win) {
    if (win->hovered == this) win->hovered = NULL;
    if (win->focused == this) win->focused = NULL;
  }
  m_children.clear();
}

void ui::view::clear(void) { m_children.clear(); }



void ui::view::repaint(bool do_invalidate) {
  // if we aren't mounted, don't paint - it doesn't make sense
  if (window() == NULL) return;


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
  each_child(fn(auto &c) { c.repaint(false); });
  if (do_invalidate) invalidate();
}


void ui::view::set_focused(void) {
  auto *win = window();
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
  // printf("mouse move %d %d   %d %d\n", left(), top(), ev.x, ev.y);
  ev.x -= left();
  ev.y -= top();

  // what we do here is based on what kind of event we have.
  bool sent_to_child = false;
  each_child(fn(auto &child) {
    if (sent_to_child) return;
    // if the mouse event is within the child, send it there instead
    auto crel = child.relative();
    if (crel.contains(ev.x, ev.y)) {
      child.dispatch_mouse_event(ev);
      sent_to_child = true;
    }
  });

  if (!sent_to_child) {
    if (ev.left) on_left_click(ev);
    if (ev.right) on_right_click(ev);
    if (ev.ds) on_scroll(ev);

    if (ev.dx || ev.dy) on_mouse_move(ev);
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
  gfx::scribe s(window()->bmp());

  s.enter();
  auto r = absolute_rect();

  s.state().clip = r;
  s.state().offset = gfx::point(r.x, r.y);

  return s;
}


// invalidate the whole view
void ui::view::invalidate(void) { window()->invalidate(absolute_rect()); }




void ui::view::do_reflow(void) {
  // always just ask the window to reflow.
  window()->reflow();
}


/*
void ui::view::set_pos(ui::Direction dir, int pos) {
  switch (dir) {
    case ui::Direction::Vertical:
      m_rel.y = pos;
      break;
    case ui::Direction::Horizontal:
      m_rel.x = pos;
      break;
  }
  return;
}
*/

ui::SizePolicy ui::view::get_size_policy(ui::Direction dir) {
  return dir == ui::Direction::Vertical ? get_height_policy() : get_width_policy();
}


int ui::view::size(ui::Direction dir) { return dir == ui::Direction::Vertical ? height() : width(); }


void ui::view::set_size(ui::Direction dir, float sz) {
  switch (dir) {
    case ui::Direction::Vertical:
      flex_item_set_height(m_fi, sz);
      break;
    case ui::Direction::Horizontal:
      flex_item_set_width(m_fi, sz);
      break;
  }
  return;
}


void ui::view::set_size(float w, float h) {
  flex_item_set_width(m_fi, w);
  flex_item_set_height(m_fi, h);
}

/*
void ui::view::set_pos(int x, int y) {
  m_rel.x = x;
  m_rel.y = y;
}
*/

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
  r.x = get_flex_padding_left();
  r.y = get_flex_padding_top();
  r.w = width() - r.x - get_flex_padding_right();
  r.h = height() - r.y - get_flex_padding_bottom();

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
  flex_item_add(m_fi, v->m_fi);
  if (window()) {
    do_reflow();
    recurse_mount(v);
  }
}
