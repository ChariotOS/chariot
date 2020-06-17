#include <gui/window.h>
#include <gui/application.h>


gui::window::window(int id, ck::string name, gfx::rect r) {
	m_id = id;
	m_name = name;
	m_rect = r;
}

