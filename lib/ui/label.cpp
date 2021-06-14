#include <gfx/font.h>
#include <gfx/scribe.h>
#include <ui/label.h>




class word : public ui::view {
  /*
   * what size did we calculate for?
   * (what was the last return value of get_font_size())
   */
  float calculated_font_size = NAN;

  ck::string m_word;

 public:
  word(ck::string word) : m_word(word) {
    set_background(None);
    // set_flex_shrink(0.0);
    // set_flex_grow(0.0);
  }


  void calculate(void) {
    auto font = get_font();
    calculated_font_size = get_font_size();

    font->with_line_height(calculated_font_size, [&]() { auto space_size = font->width(' '); });
  }

  virtual void mounted(void) { calculate(); }

  virtual void flex_self_sizing(float &width, float &height) {
    float current_size = get_font_size();
    if (calculated_font_size != current_size) {
      calculate();
      // height = get_flex_width();
    }
  }

  virtual void paint_event(void) {
    auto s = get_scribe();
    int x = 0;
    int y = 0;
    auto font = get_font();
    auto rect = relative();

    rect.x = 0;
    rect.y = 0;



    font->with_line_height(get_font_size(), [&]() {
      auto p = gfx::printer(s, *font, rect.x, rect.y + font->ascent(), rect.w);
      p.set_color(get_foreground());
      p.write_utf8(m_word.get());
    });
  }
};




ui::label::label(ck::string contents, ui::TextAlign align) : m_align(align) {
#if 0
  set_flex_wrap(ui::FlexWrap::Wrap);
  set_size(NAN, NAN);


  set_flex_align_items(ui::FlexAlign::Start);
  set_flex_align_content(ui::FlexAlign::Start);
  set_flex_direction(ui::FlexDirection::Row);

  set_flex_shrink(0.0);
  // set_flex_grow(1.0);

  set_text(contents);
#endif
}

ui::label::~label(void) {}

#if 0
void ui::label::flex_self_sizing(float &width, float &height) {
  int max_bottom = 0;

  int lines = 1;
  int along = 0;
  int lh = 0;

  each_child([&](ui::view &v) {
    lh = v.get_flex_height();

    int w = v.get_flex_basis();
    if (w + along > width) {
      lines++;
      along = 0;
    }

    along += w;
  });
  height = lines * lh;
}
#endif

void ui::label::set_text(ck::string contents) {
  auto parts = contents.split(' ');

  /* clear out the children */
  this->clear();

  for (auto &part : parts) {
    this->add(new word(part));
  }
}


void ui::label::paint_event(void) {}
