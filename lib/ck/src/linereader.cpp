#include <ck/linereader.h>
#include <stdio.h>



ck::linereader::linereader(ck::stream &s) : m_stream(s) {
}


ck::option<ck::string> ck::linereader::next(void) {
  ck::string buf;

  while (!m_stream.eof()) {
    int c = m_stream.getc();
    if (c == EOF) break;
    if (c == '\n') break;
    buf.push(c);
  }

  return Some(buf);
}

ck::option<ck::string> ck::linereader::next(const ck::string &prompt) {
  printf("%s", prompt.get());
  fflush(::stdout);
  return next();
}
