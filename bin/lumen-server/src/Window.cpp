#include <lumen-server/Window.h>
#include <lumen-server/ApplicationConnection.h>
namespace server {
  Window::Window(ApplicationConnection& connection) : m_connection(connection) {
    static long next_window_id = 0;
    this->m_id = next_window_id++;
  }

  Window::~Window(void) { printf("Window '%s' was destroyed\n", name().get()); }
}  // namespace server
