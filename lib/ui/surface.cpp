#include <ui/surface.h>
#include <ui/view.h>

void ui::surface::reflow() {
  auto *rv = root_view();
  rv->set_size(m_rect.w, m_rect.h);
  rv->layout();

  // ask the window to repaint. This is expensive.
  rv->repaint(false /* do not invalidate, let the view do that. */);
  did_reflow();
  // invalidate(m_rect);
}
