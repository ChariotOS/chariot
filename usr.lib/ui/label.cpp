#include <ui/label.h>
#include <gfx/font.h>

ui::label::label(ck::string contents, ui::TextAlign align) : m_contents(move(contents)), m_align(align) {}

ui::label::~label(void) {}


void ui::label::paint_event(void) {
  if (m_contents.len() == 0) return;

	// todo: FONTS!
	auto font = gfx::font::get_default();
  auto s = get_scribe();

	auto rect = gfx::rect(0, 0, width(), height());
	// printf("painting label '%s'\n", m_contents.get());
	s.draw_text(*font, rect, m_contents, m_align, 0xFFFFFF, true);
}
