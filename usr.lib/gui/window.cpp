#include <gui/window.h>
#include <gui/application.h>


gui::window::window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap> bmp) {
	m_id = id;
	m_name = name;
	m_rect = r;
	m_bitmap = bmp;
}


void gui::window::flush(void) {
}
