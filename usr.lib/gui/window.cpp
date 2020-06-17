#include <gui/window.h>
#include <gui/application.h>


gui::window::window(int id, ck::string name, gui::rect r) {
	m_id = id;
	m_name = name;
	m_rect = r;
}

