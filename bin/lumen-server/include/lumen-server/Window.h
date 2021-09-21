#pragma once
#include <ck/ptr.h>
#include <ck/string.h>

namespace server {

  // fwd decl
  class ApplicationConnection;
  class WindowManager;

  // A window is a single bitmap that is drawn to the screen at a certain location with a certain decoration
  class Window : public ck::refcounted<server::Window> {
   public:
    Window(ApplicationConnection &connection);
    ~Window();

    long id() const { return m_id; }

    const ck::string &name(void) const { return m_name; }
    void set_name(ck::string &name) {
      m_name = name.get();
      // TODO: redraw the window
    }

    const ApplicationConnection &connection() const { return m_connection; }
    ApplicationConnection &connection() { return m_connection; }

   private:
    ck::string m_name = "Window";
    long m_id = 0;
    ApplicationConnection &m_connection;
  };

}  // namespace server