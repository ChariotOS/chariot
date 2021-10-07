#pragma once


// this api is just C++, so C can't access it :(
#ifdef __cplusplus

#include <ck/object.h>
#include <ck/socket.h>
#include <lumen/msg.h>


// shared functionality between the client and server
namespace lumen {


  ck::vec<lumen::msg *> drain_messages(ck::IPCSocket &sock, bool &failed);

};  // namespace lumen

#endif
