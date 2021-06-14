#include <ui/view.h>
#include <ui/window.h>



static unsigned long view_id = 0;

ui::view::view() {
  // printf("view %d created\n", view_id++);
}

ui::view::~view(void) {
  auto *srf = surface();
  if (srf) {
    if (srf->hovered == this) srf->hovered = NULL;
    if (srf->focused == this) srf->focused = NULL;
  }
  m_children.clear();
}

void ui::view::clear(void) { m_children.clear(); }



void ui::view::repaint(gfx::rect &relative_dirty_region) {
  // printf("paint %3d %3d %3d %3d\n", m_relative_rect.x, m_relative_rect.y, m_relative_rect.w,
  //        m_relative_rect.h);

  // if we aren't mounted, don't paint - it doesn't make sense
  if (surface() == NULL) return;

  auto s = get_scribe();
  s.state().clip.x += relative_dirty_region.x;
  s.state().clip.y += relative_dirty_region.y;
  s.state().clip.w = relative_dirty_region.w;
  s.state().clip.h = relative_dirty_region.h;

  s.fill_rect(rect(), get_background());

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


  gfx::scribe sc = get_scribe();
  paint_event(sc);

  each_child([&relative_dirty_region](ui::view &c) {
    auto r = c.relative();
    if (r.intersects(relative_dirty_region)) {
      auto new_region = relative_dirty_region.shifted(-r.x, -r.y);
      c.repaint(new_region);
    }
  });
}


static void indent(int depth) {
  for (int i = 0; i < depth; i++)
    printf("   >");
}

void ui::view::dump_hierarchy(int depth) {
  indent(depth);
  printf("%d %d %d %d\n", m_relative_rect.x, m_relative_rect.y, m_relative_rect.w,
         m_relative_rect.h);

  for (auto &child : m_children) {
    child->dump_hierarchy(depth + 1);
  }
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
void ui::view::update(void) {
  auto *win = surface();

  if (win == nullptr) return;
  gfx::rect r = rect();
  repaint(r);

  r = absolute_rect();
  surface()->invalidate(r);
}


void ui::view::do_reflow(void) {
  if (surface()) {
    // always just ask the window to reflow.
    surface()->reflow();
  }
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



static void recurse_mount(ui::view *v) {
  v->mounted();
  v->each_child([](ui::view &other) { recurse_mount(&other); });
};


ui::view &ui::view::add(ui::view *v) {
  v->m_parent = this;
  m_children.push(ck::unique_ptr(v));
  // add the child's flexbox on the end
  // flex_item_add(m_fi, v->m_fi);
  if (surface()) {
    do_reflow();
    recurse_mount(v);
  }
  return *v;
}



void ui::view::set_layout(ck::ref<ui::layout> l) {
  if (m_layout) {
    m_layout->notify_disowned(*this);
    // m_layout->remove_from_parent();
  }
  m_layout = move(l);
  if (m_layout) {
    m_layout->notify_adopted(*this);
    run_layout();
  } else {
    update();
  }
}


void ui::view::run_layout(void) {
  each_child([&](auto &child) { child.run_layout(); });

  custom_layout();
  if (!m_layout) return;

  m_layout->run(*this);

  reflowed();
  update();
}


void ui::view::set_relative_rect(gfx::rect &a_rect) {
  // Get rid of negative width/height values.
  gfx::rect rect = {a_rect.x, a_rect.y, MAX(a_rect.w, 0), MAX(a_rect.h, 0)};

  m_relative_rect = rect;

  // TODO:
  // if (rect == m_relative_rect) return;
}
