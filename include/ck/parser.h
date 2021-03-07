#pragma once

#include <ck/string.h>

namespace ck {

#define TOK_ERR (-2)
#define TOK_EOF (-1)

  struct token {
    int start, end;
    int type = TOK_EOF;
    ck::string value;
    /* TODO: extents, etc... */

    ck::string format(void);

    explicit inline token(int type, ck::string value, int start, int end)
        : type(type), value(value), start(start), end(end) {
    }
  };

  /* base class, to be implemented by your workload */
  class lexer {
   protected:
    off_t prev_start = 0;
    off_t index = 0;
    ck::string_view source;
    token lex_num(int type);
    /* EOF on EOF, ofc :^) */
    int next();
    int peek();

    inline bool in_charset(char c, const char *set) {
      for (int i = 0; set[i]; i++)
        if (set[i] == c) return true;
      return false;
    }

   public:
    inline ck::token tok(int type, ck::string value) {
      auto t = ck::token(type, value, prev_start, index - 1);
      prev_start = index;
      return t;
    }
    inline lexer(const ck::string_view &v) : source(v) {
      prev_start = index = 0;
    };
    virtual ~lexer(void);
    virtual ck::token lex() = 0;

    void skip_spaces(void);
  };




};  // namespace ck
