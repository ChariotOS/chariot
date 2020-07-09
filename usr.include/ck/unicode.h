#pragma once

#include <stdint.h>
#include <string.h>

namespace ck {


  inline unsigned utf8_to_unicode(char *line, unsigned index, unsigned len,
                                  uint32_t &res) {
    unsigned value;
    unsigned char c = line[index];
    unsigned bytes, mask, i;

    if (c == '\0') return 0;

    res = c;
    line += index;
    len -= index;

    /*
     * 0xxxxxxx is valid utf8
     * 10xxxxxx is invalid UTF-8, we assume it is Latin1
     */
    if (c < 0xc0) return 1;

    /* Ok, it's 11xxxxxx, do a stupid decode */
    mask = 0x20;
    bytes = 2;
    while (c & mask) {
      bytes++;
      mask >>= 1;
    }

    /* Invalid? Do it as a single byte Latin1 */
    if (bytes > 6) return 1;
    if (bytes > len) return 1;

    value = c & (mask - 1);

    /* Ok, do the bytes */
    for (i = 1; i < bytes; i++) {
      c = line[i];
      if ((c & 0xc0) != 0x80) return 1;
      value = (value << 6) | (c & 0x3f);
    }
    res = value;
    return bytes;
  }


  // Iterate over each codepoint in a utf8 string
  template <typename Fn>
  inline void each_codepoint(char *utf8, unsigned len, Fn cb) {
    uint32_t cp = 0;
    unsigned index = 0;
    while (1) {
      auto n = utf8_to_unicode(utf8, index, len, cp);

      if (n == 0) break;
      cb(cp, index);
      index += n;
    }
  }

};  // namespace ck
