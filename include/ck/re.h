#pragma once

#include <regex.h>

namespace ck {


	// regular expression state
	class re {
  	regex_t regex;
		bool valid = false;
		public:
			re();

			bool compile(const char *expr, int flags = REG_EXTENDED);

			bool matches(const char *text);

			static bool matches(const char *regex, const char *text);

			// bool matches(const char *text);
	};
}
