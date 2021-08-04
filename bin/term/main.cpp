
#include <ui/application.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <ck/dir.h>
#include <ck/timer.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/image.h>
#include <gfx/scribe.h>
#include <sys/sysbind.h>
#include <ui/window.h>
#include <ui/view.h>
#include <ui/boxlayout.h>
#include <ck/time.h>
#include "terminalview.h"

extern const char **environ;

class terminal_window : public ui::window {
 public:
  terminal_window(void) : ui::window("Terminal", 400, 300) {
    // set_theme(0x353535, 0xFFFFFF, 0x353535);
    // compositor_sync(true);
  }

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    return ui::make<terminalview>();

    // clang-format on
  }
};



int main() {
  // connect to the window server
  ui::application app;

  auto win = terminal_window();
  // win.compositor_sync(true);
  win.set_double_buffer(false);

  // start the application!
  app.start();
  return 0;
}
