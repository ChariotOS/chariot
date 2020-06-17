#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <gui/rect.h>


namespace gui {

  // fwd decl
  class appication;

  class window : public ck::object {
   public:
    window(int id, ck::string name, gui::rect r);

    CK_OBJECT(gui::window);

   private:
    long m_id;
    ck::string m_name;
		gui::rect m_rect;
  };
};  // namespace gui
