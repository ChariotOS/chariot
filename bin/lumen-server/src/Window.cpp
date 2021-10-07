#include <lumen-server/Window.h>
#include <lumen-server/ApplicationConnection.h>
#include <lumen-server/WindowManager.h>

namespace server {



  Window::Window(WindowManager& wm) : m_wm(wm) {
    static long next_window_id = 0;
    this->m_id = next_window_id++;
  }


  void Window::show() { wm().show(this); }
  void Window::hide() { wm().hide(this); }


  AppWindow::AppWindow(ApplicationConnection& connection) : Window(connection.window_manager()), m_connection(connection) {}

  AppWindow::~AppWindow(void) { printf("AppWindow '%s' was destroyed\n", name().get()); }
}  // namespace server
