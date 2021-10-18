#pragma once

struct scheme;

namespace scm {
	class Scheme {
		public:
			Scheme(void);
			~Scheme(void);

			void eval(const char *expr);


			void load(const char *path, const char *name = nullptr);

		private:
			struct scheme *sc;
	};
};
