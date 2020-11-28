#pragma once


// ew, gross
#include "../usr.lib/wren/src/include/wren.hpp"


#ifdef __cplusplus

#include <ck/object.h>

namespace wren {
  class vm : public ck::object {
		WrenVM *m_vm;

		public:


			vm(void);
			~vm(void);
			WrenInterpretResult run_file(const char *path);
			WrenInterpretResult run_expr(const char *expr);

	};
};  // namespace wren

#endif
