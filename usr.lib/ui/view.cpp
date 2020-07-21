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
