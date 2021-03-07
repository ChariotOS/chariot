#include <ck/parser.h>
#include <stdio.h>


ck::string ck::token::format(void) {
  ck::string s = "{";
  char buf[16];
  snprintf(buf, 16, "%d", type);
  s += buf;
  s += " ";
  s += value;
  s += "}";
  return s;
}


ck::lexer::~lexer(void) { /* nothin, for now */
}


int ck::lexer::next() {
  auto c = peek();
  index++;
  return c;
}

int ck::lexer::peek() {
  if (index > source.len()) {
    return EOF;
  }
  return source[index];
}

void ck::lexer::skip_spaces(void) {
  while (in_charset(peek(), " \n\t")) {
    auto v = next();
    if (v == EOF || v == 0) break;
    prev_start++;
  }
}
