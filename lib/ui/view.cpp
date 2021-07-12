#include <ui/view.h>
#include <ui/window.h>



static unsigned long view_id = 0;

ui::view::view() {}

// ui::view::view(ck::vec<ck::unique_ptr<ui::view>> children) {
//   for (auto &c : children) {
//     add(c.leak_ptr());
//   }
// }
ui::view::view(std::initializer_list<ui::view *> children) {
  for (auto *c : children) {
    add(c);
  }
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
  // if we aren't mounted, don't paint - it doesn't make sense
  if (surface() == NULL) return;

  // printf("repaint w:%3d h:%3d\n", width(), height(), get_background());

  auto s = get_scribe();
  s.state().clip.x += relative_dirty_region.x;
  s.state().clip.y += relative_dirty_region.y;
  s.state().clip.w = relative_dirty_region.w;
  s.state().clip.h = relative_dirty_region.h;

  // draw the border
  gfx::rect border = gfx::rect(0, 0, width(), height());

  for (int i = 0; i < get_bordersize(); i++) {
    s.draw_rect(border, get_bordercolor());
    border.x += 1;
    border.y += 1;
    border.w -= 2;
    border.h -= 2;
  }

  if (has_background()) s.fill_rect(border, get_background());

  paint_event(s);

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
  printf("%-4d %-4d %-4d %-4d\n", m_relative_rect.x, m_relative_rect.y, m_relative_rect.w,
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
void ui::view::update(void) { update(absolute_rect()); }

void ui::view::update(gfx::rect dirty_area) {
  auto *win = surface();
  if (win == nullptr) return;
  win->invalidate(dirty_area);
}


void ui::view::update_layout(void) {
  if (surface()) {
    // always just ask the surface to reflow.
    surface()->schedule_reflow();
  }
}


gfx::rect ui::view::absolute_rect(void) {
  gfx::rect r = relative();
  if (parent() != NULL) {
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


ui::view &ui::view::add(ck::ref<ui::view> v) {
  v->m_parent = this;
  m_children.push(v);
  if (surface()) {
    update_layout();
    recurse_mount(v);
  }
  return *v;
}



void ui::view::set_layout(ck::ref<ui::layout> l) {
  if (m_layout) {
    m_layout->notify_disowned(*this);
  }
  m_layout = move(l);
  if (m_layout) {
    m_layout->notify_adopted(*this);
    run_layout();
  } else {
    update();
  }
}


void ui::view::run_layout(int depth) {
  custom_layout();


  // layout where each child will go if a layout is provided.
  if (layout()) layout()->run(*this);

  // then layout each child with their new sizes that we decided
  // by this parent
  for (auto &child : m_children) {
    child->run_layout(depth + 1);
  }

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


void ui::view::load_style(const ui::style &cfg) {
  if (cfg.background) {
    set_background(cfg.background.unwrap());
  }

  if (cfg.foreground) {
    set_foreground(cfg.foreground.unwrap());
  }

  if (cfg.bordersize) {
    set_bordersize(cfg.bordersize.unwrap());
  }

  if (cfg.bordercolor) {
    set_bordercolor(cfg.bordercolor.unwrap());
  }


  if (cfg.layout) {
    set_layout(cfg.layout);
  }

  if (cfg.margins) {
    set_margins(cfg.margins.unwrap());
  }
  if (cfg.padding) {
    set_padding(cfg.padding.unwrap());
  }
}


void ui::view::handle_animation_frame(void) { printf("animation frame!\n"); }