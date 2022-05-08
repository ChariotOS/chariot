#include <lumen-server/ApplicationConnection.h>
#include "chariot/ck/ptr.h"



namespace server {


  ApplicationConnection::ApplicationConnection(ck::ref<ck::ipcsocket> s) : lumen::server_connection_stub(s) {
    // TODO: register this connection with the desktop object
    printf("ApplicationConnection Constructed\n");
  }


  ApplicationConnection::~ApplicationConnection() {
    printf("ApplicationConnection Destructed\n");
    // TODO - Notify the window server of the connection closing
    //        so it can remove any references to this application
  }


  ck::option<lumen::client_create_window_response> ApplicationConnection::on_create_window(void) {
		printf("create window!\n");
    auto window = ck::make_ref<AppWindow>(*this);
    m_windows.set(window->id(), window);
    return {window->id()};
  }


  ck::option<lumen::client_set_window_name_response> ApplicationConnection::on_set_window_name(uint32_t wid, ck::string name) {
    printf("wid: %d\n", wid);
    if (!m_windows.contains(wid)) return {-ENOENT};
    printf("set name to %s\n", name.get());
    auto window = m_windows.get(wid);

    window->set_name(name);

    return {0};
  }



  void ApplicationConnection::set_window_manager(WindowManager *wm) {

		printf("set wm %p\n" ,wm);
		this->wm = wm;
	}


}  // namespace server
