#include <ui/surface.h>
#include <ui/view.h>
#include "ck/eventloop.h"
#include <sys/sysbind.h>


ui::surface::surface(void) {}


void ui::surface::request_animation_frame(ck::weak_ref<ui::view> view) {
  m_views_wanting_animation.push(view);

  if (!m_animation_timer.running()) {
    // start the tick
    m_animation_timer.start(1000 / 60, false);
    m_animation_timer.on_tick = [this] {
      // copy the vector. Not smart lol, but it works and avoids race problems if the view
      auto views_wanting_animation = m_views_wanting_animation;
      m_views_wanting_animation.clear();
      for (auto &view : views_wanting_animation) {
        ui::view::ref v = view.get();
        if (v) v->handle_animation_frame();
      }
    };
  }
}

void ui::surface::do_reflow(void) {
  // printf("do reflow!\n");
  auto *rv = root_view();
  rv->set_fixed_size({width(), height()});
  rv->set_width(width());
  rv->set_height(height());

  rv->run_layout();
  did_reflow();
}

void ui::surface::schedule_reflow() {
  ck::eventloop::defer_unique("ui::surface::schedule_reflow", [this]() { this->do_reflow(); });
}
