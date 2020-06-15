#pragma once

#include <ck/string.h>
#include <ck/vec.h>
#include <stdio.h>

namespace ck {
  /**
   * prompt - the state for reading lines with a pretty prompt
	 *          and nice bindings and whatnot.
   */
  class prompt {
   public:
		prompt(void);
		~prompt(void);
    ck::string readline(const char *prompt);

    void add_history_item(ck::string);

   private:

		void insert(char c);
		void del(void);
		void display(const char *prompt);

		void handle_special(char c);


		int ind = 0;
		int len = 0;
		int max = 0;
		char *buf;


    ck::vec<ck::string> m_history;
  };
};  // namespace ck
