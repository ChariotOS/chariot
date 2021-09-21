#pragma once

#include <lumen/ipc.h>
#include <lumen-server/Window.h>

namespace server {


  class WindowManager;

  // This represents a single connection from an application.
  // An application can technically be connected over multipled
  // sockets, but typically that is not the expected use case.
  //
  // This structure is event based, and certain methods are called
  // when a client application sends a message to the window server.
  // A connection has weak references to window objects, which are
  // held
  class ApplicationConnection : public lumen::server_connection_stub {
   public:
    ApplicationConnection(ck::ref<ck::ipcsocket> s);
    virtual ~ApplicationConnection();

    ck::option<lumen::client_create_window_response> on_create_window(void) override;
    ck::option<lumen::client_set_window_name_response> on_set_window_name(uint32_t wid, ck::string name) override;


    // set and get the WindowManger for this session
    void set_window_manager(WindowManager *wm);
    const WindowManager &window_manager() const { return *wm; }
    WindowManager &window_manager() { return *wm; }


   protected:
    friend server::Window;


   private:
    WindowManager *wm = NULL;
    uint32_t m_next_wid = 0;
    ck::map<uint32_t, ck::ref<Window>> m_windows;
  };


  class Server : public ck::ipc::server<ApplicationConnection> {
   public:
    Server(WindowManager &wm);
    ~Server(void);

    virtual void on_connection(ck::ref<ApplicationConnection> c);
    virtual void on_disconnection(ck::ref<ApplicationConnection> c);


   protected:
    friend class WindowManager;
    WindowManager &wm;
  };


}  // namespace server