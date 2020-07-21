#pragma once

#include <ck/io.h>
#include <ck/vec.h>
#include <ck/option.h>

namespace ck {
  class linereader {
   public:
    linereader(ck::stream &);


		// run with no prompt
		ck::option<ck::string> next();

		// run with a prompt
		ck::option<ck::string> next(const ck::string &prompt);

   private:


   private:

		bool m_echo = false;

    ck::stream &m_stream;
    ck::vec<char> m_buf;
    // where in the buffer are we inserting?
    uint32_t m_cursor = 0;
  };
};  // namespace ck
