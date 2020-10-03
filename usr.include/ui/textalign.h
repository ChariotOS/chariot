#pragma once


namespace ui {
#define UI_ENUMERATE_TEXT_ALIGNMENTS(M) \
  M(TopLeft)                            \
  M(CenterLeft)                         \
  M(Center)                             \
  M(CenterRight)                        \
  M(TopRight)                           \
  M(BottomRight)
  enum class TextAlign {
#define __ENUMERATE(x) x,
    UI_ENUMERATE_TEXT_ALIGNMENTS(__ENUMERATE)
#undef __ENUMERATE
  };
}  // namespace ui
