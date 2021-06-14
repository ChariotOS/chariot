#include <ui/surface.h>
#include <ui/view.h>

void ui::surface::reflow() {
  auto *rv = root_view();
  rv->set_fixed_size({m_rect.w, m_rect.h});
  rv->set_width(m_rect.w);
  rv->set_height(m_rect.h);

  rv->run_layout();
  did_reflow();

  rv->update();
  // invalidate(m_rect);
}
