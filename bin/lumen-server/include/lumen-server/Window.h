#pragma once
#include <ck/ptr.h>
#include <ck/string.h>

namespace server {

  // fwd decl
  class ApplicationConnection;
  class WindowManager;


  // a `Window` class is the base class for all rectangles managed by the WindowManager.
  class Window : public ck::refcounted<server::Window> {
   public:
    Window(WindowManager &wm);
    virtual ~Window() = default;


    // get the window manager for this window.
    const WindowManager &wm() const { return m_wm; }
    WindowManager &wm() { return m_wm; }
    const ck::string &name(void) const { return m_name; }
    void set_name(ck::string &name) {
      m_name = name.get();
      // TODO: redraw the window
    }

    long id() const { return m_id; }


    void show();
    void hide();

    // events handled by subclasses
    virtual void onShow(void) {}
    virtual void onHide(void) {}


   private:
    WindowManager &m_wm;

    ck::string m_name = "Application";
    long m_id = 0;
  };

  // an `AppWindow` is a window managed by some application over IPC
  class AppWindow final : public Window {
   public:
    AppWindow(ApplicationConnection &connection);
    virtual ~AppWindow();




    const ApplicationConnection &connection() const { return m_connection; }
    ApplicationConnection &connection() { return m_connection; }

   private:
    ApplicationConnection &m_connection;
  };

}  // namespace server