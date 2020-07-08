#include <ui/view.h>
#include <ui/window.h>



ui::view::view() {
}

ui::view::~view(void) {
  for (auto &v : m_children) {
    delete &v;
  }
}



void ui::view::repaint(void) {
}


bool ui::view::event(ui::event &ev) {
  // try to handle each type
  switch (ev.get_type()) {
    case ui::event::type::mouse: {
      // translate the mouse event to maybe within this view
      ui::mouse_event mv = ev.as<ui::mouse_event>();
      mv.x -= m_rel.x;
      mv.y -= m_rel.y;
      if (m_rel.contains(mv.x, mv.y)) {
        if (mouse_event(mv)) return true;
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
	// printf("do reflow!\n");
  /*
   * reflow all the children so we can get an idea of their sizes
   * we do this because the layout of the parent is dependent on the
   * size of the children
   */
  each_child(fn(auto &c) { c.do_reflow(); });


  // now, the hard part.


	// repaint this view after we did a reflow :)
	repaint();
}
