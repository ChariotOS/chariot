#include <ui/surface.h>
#include <ui/view.h>
#include "ck/eventloop.h"
#include <sys/sysbind.h>

static void do_surface_reflow(ui::surface *s) {}


void ui::surface::do_reflow(void) {
  // printf("do reflow!\n");
  auto *rv = root_view();
  rv->set_fixed_size({width(), height()});
  rv->set_width(width());
  rv->set_height(height());

  // printf("running layout.\n");
  rv->run_layout();
  // printf("ran layout.\n");
  did_reflow();

  // rv->dump_hierarchy();

  // rv->update();
}

void ui::surface::schedule_reflow() {
  ck::eventloop::defer_unique("ui::surface::schedule_reflow", [this]() { this->do_reflow(); });
}
