#pragma once


#include <lumen/ipc_core.h>


namespace lumen {
  // implemented in libui/lumen.cpp, as with most other lumen client operations
  class Connection : public lumen::client_connection_stub {
   public:
    Connection(ck::ref<ck::IPCSocket> s) : lumen::client_connection_stub(move(s)) {}

    virtual ~Connection() { printf("Lumen Client Connection Destructed\n"); }
  };

}  // namespace lumen