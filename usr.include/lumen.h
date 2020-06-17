#pragma once


// this api is just C++, so C can't access it :(
#ifdef __cplusplus

#include <ck/object.h>
#include <lumen/msg.h>
#include <ck/socket.h>

// client namespace
namespace lumen {

	// TODO: event loop this thing
	class session : public ck::object {

		// this is the connection to the windowserver
		ck::localsocket sock;

		public:

		session(void);
		~session(void);

		inline bool connected(void) {
			return sock.connected();
		}

		long send_raw(int type, void *payload, size_t payloadsize);


		// send a bare message
		inline int send_msg(int type) {
			return send_raw(type, NULL, 0);
		}

		template<typename T>
			inline int send_msg(int type, const T& payload) {
				return send_raw(type, (void*)&payload, sizeof(payload));
			}


		CK_OBJECT(lumen::session);
	};
};

#endif
