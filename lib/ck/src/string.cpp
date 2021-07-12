#include <ck/string.h>
#include <stdio.h>


extern "C" int vfctprintf(
    void (*out)(char character, void *arg), void *arg, const char *format, va_list va);


static void scribe_draw_text_callback(char c, void *arg) {
  auto &f = *(ck::string *)arg;
  f += c;
}

template <>
ck::string ck::basic_string<char>::format(const char *fmt, ...) {
  string dst;

  va_list va;
  va_start(va, fmt);
  vfctprintf(scribe_draw_text_callback, (void *)&dst, fmt, va);
  va_end(va);

  return dst;
}


template <>
void ck::basic_string<char>::appendf(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  vfctprintf(scribe_draw_text_callback, (void *)this, fmt, va);
  va_end(va);
}
