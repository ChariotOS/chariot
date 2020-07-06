#include <gui/window.h>
#include <gui/application.h>


gui::window::window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap> bmp) {
	m_id = id;
	m_name = name;
	m_rect = r;
	m_bitmap = bmp;
}


gui::window::~window(void) {
}

void gui::window::flush(void) {
	auto &app = gui::application::get();
	struct lumen::invalidate_msg iv;
	iv.id = m_id;
	iv.x = 0;
	iv.y = 0;
	iv.w = m_rect.w;
	iv.h = m_rect.h;
	app.send_msg(LUMEN_MSG_WINDOW_INVALIDATE, iv);
}
