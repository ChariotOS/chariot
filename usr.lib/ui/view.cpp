#include <ui/view.h>
#include <ui/window.h>



ui::view::view() {}

ui::view::~view(void) {
  if (m_window->hovered == this) m_window->hovered = NULL;
  if (m_window->focused == this) m_window->focused = NULL;
  for (auto &v : m_children) {
    delete &v;
  }
}



void ui::view::repaint(void) {
  if (m_use_bg) {
    auto s = get_scribe();
    s.clear(m_background);
  }

  paint_event();

  each_child(fn(auto &c) { c.repaint(); });
}


void ui::view::set_focused(void) {
	if (m_window->focused) {
		m_window->focused->on_blur();
	}


	m_window->focused = this;
	on_focused();
}


void ui::view::dispatch_mouse_event(ui::mouse_event &event) {
  // copy the event and adjust it to the local bounds
  ui::mouse_event ev = event;
  ev.x -= m_rel.x;
  ev.y -= m_rel.y;

  // what we do here is based on what kind of event we have.
  bool sent_to_child = false;
  each_child(fn(auto &child) {
    if (sent_to_child) return;
    // if the mouse event is within the child, send it there instead
    auto &crel = child.m_rel;
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
  // try to handle each type
  switch (ev.get_type()) {
    case ui::event::type::mouse: {
      // translate the mouse event to maybe within this view
      ui::mouse_event mv = ev.as<ui::mouse_event>();
      if (m_rel.contains(mv.x, mv.y)) {
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
    if (v.event(ev)) return true;
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
  /*
   * reflow all the children so we can get an idea of their sizes
   * we do this because the layout of the parent is dependent on the
   * size of the children
   */
  each_child(fn(auto &c) { c.do_reflow(); });

  // now, the hard part.
  reflow_impl();

  // repaint this view after we did a reflow :)
  repaint();
}




void ui::view::set_background(uint32_t bg) {
  m_background = bg;
  m_use_bg = true;
  repaint();
}

void ui::view::remove_background() {
  m_use_bg = false;
  repaint();
}


void ui::view::set_pos(ui::direction dir, int pos) {
  switch (dir) {
    case ui::direction::vertical:
      m_rel.y = pos;
      break;
    case ui::direction::horizontal:
      m_rel.x = pos;
      break;
  }
  return;
}
ui::size_policy ui::view::get_size_policy(ui::direction dir) {
  return dir == ui::direction::vertical ? get_height_policy()
                                        : get_width_policy();
}


int ui::view::size(ui::direction dir) {
  return dir == ui::direction::vertical ? height() : width();
}



void ui::view::set_size(ui::direction dir, int sz) {
  switch (dir) {
    case ui::direction::vertical:
      m_rel.h = sz;
      break;
    case ui::direction::horizontal:
      m_rel.w = sz;
      break;
  }
  return;
}



void ui::view::set_size(int w, int h) {
  m_rel.w = w;
  m_rel.h = h;
}

void ui::view::set_pos(int x, int y) {
  m_rel.x = x;
  m_rel.y = y;
}

gfx::rect ui::view::absolute_rect(void) {
  if (m_parent != NULL) {
    gfx::rect p = parent()->absolute_rect();
    gfx::rect r = m_rel;
    r.x += p.x;
    r.y += p.y;
    return r;
  }
  return m_rel;
}

