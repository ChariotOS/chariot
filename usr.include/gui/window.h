#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <gfx/rect.h>
#include <gfx/bitmap.h>

namespace gui {

  // fwd decl
  class appication;

  class window : public ck::object {
    CK_OBJECT(gui::window);

   public:
    window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap>);

		void flush(void);


		inline gfx::bitmap &bmp(void) {
			return *m_bitmap;
		}

   private:
    long m_id;
    ck::string m_name;
		gfx::rect m_rect;
		ck::ref<gfx::shared_bitmap> m_bitmap;
  };
};  // namespace gui
