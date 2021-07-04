#include <gfx/font.h>
#include <gfx/scribe.h>
#include <ui/label.h>




// class word : public ui::view {
//   /*
//    * what size did we calculate for?
//    * (what was the last return value of get_font_size())
//    */
//   float calculated_font_size = NAN;

//   ck::string m_word;

//  public:
//   word(ck::string word) : m_word(word) { set_background(None); }


//   void calculate(void) {
//     auto font = get_font();
//     calculated_font_size = get_font_size();

//     font->with_line_height(calculated_font_size, [&]() { auto space_size = font->width(' '); });
//   }

//   virtual void mounted(void) { calculate(); }

//   virtual void flex_self_sizing(float &width, float &height) {
//     float current_size = get_font_size();
//     if (calculated_font_size != current_size) {
//       calculate();
//       // height = get_flex_width();
//     }
//   }

//   virtual void paint_event(void) {
//     auto s = get_scribe();
//     int x = 0;
//     int y = 0;
//     auto font = get_font();
//     auto rect = relative();

//     rect.x = 0;
//     rect.y = 0;



//     font->with_line_height(get_font_size(), [&]() {
//       auto p = gfx::printer(s, *font, rect.x, rect.y + font->ascent(), rect.w);
//       p.set_color(get_foreground());
//       p.write_utf8(m_word.get());
//     });
//   }
// };




ui::label::label(ck::string content, ui::TextAlign align) : m_align(align) { set_content(content); }

ui::label::~label(void) {}



void ui::label::set_content(const ck::string &s) {
  m_content = s;
  size_to_fit();
}

void ui::label::set_autosize(bool a) {
  m_autosize = a;
  size_to_fit();
}

void ui::label::size_to_fit(void) {
  int font_size = get_font_size();
  set_min_height(font_size);

  if (is_autosize()) {
    auto font = get_font();

    assert(font);
    font->with_line_height(font_size, [&] { set_fixed_width(font->width(m_content.get())); });
  }
}

void ui::label::paint_event(gfx::scribe &s) {
  s.draw_text(*get_font(), rect(), content(), m_align, get_foreground(), true);
  s.draw_rect(rect(), 0xFF0000);
}
