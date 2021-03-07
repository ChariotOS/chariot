#pragma once

#include <template_lib.h>

namespace ui {
  enum class attr : unsigned char {
    // TODO

  };
};


template <>
struct Traits<ui::attr> {
  static unsigned long hash(ui::attr c) {
    return (unsigned long)c;
  }
  static bool equals(ui::attr a, ui::attr b) {
    return a == b;
  }
};
