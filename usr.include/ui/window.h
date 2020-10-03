#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <gfx/bitmap.h>
#include <gfx/rect.h>
#include <lumen/msg.h>
#include <ui/view.h>
#include <ck/tuple.h>

namespace ui {

  // fwd decl
  class appication;

  class window : public ck::object {
    CK_OBJECT(ui::window);

   public:
    window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap>);
    ~window();
    void flush(void);

    inline int width() { return bmp().width(); }
    inline int height() { return bmp().height(); }

    void invalidate(const gfx::rect &r, bool sync = false);

    inline gfx::bitmap &bmp(void) { return *m_bitmap; }

    void handle_input(struct lumen::input_msg &);


    template <typename T, typename... Args>
    inline T &set_view(Args &&... args) {
      m_main_view = NULL;

      ui::view *v = new T(forward<Args>(args)...);
      m_main_view = ck::unique_ptr<ui::view>(v);

      m_main_view->set_size(m_rect.w, m_rect.h);
      m_main_view->set_pos(0, 0);  // the main widget exists at the top left


      // the window can do this cause they are the window :^)
      m_main_view->m_window = this;
      m_main_view->m_parent = NULL;

      // make this main widget focused
      m_main_view->set_focused();

      // do the reflow asap (not deferred)
      m_main_view->do_reflow();



      return *(T *)v;
    }

		inline void defer_invalidation(bool d) {
			m_defer_invalidation = d;
		}


    // the whole window needs reflowed, so schedule one
    void schedule_reflow();

		ck::tuple<int, int> resize(int width, int height);


    // which view is hovered and focused?
    ui::view *focused = NULL;
    ui::view *hovered = NULL;


   private:
    bool m_pending_reflow = false;

    ck::unique_ptr<ui::view> m_main_view;

		ck::vec<gfx::rect> m_pending_invalidations;

    long m_id;
    ck::string m_name;
    gfx::rect m_rect;

		bool m_defer_invalidation = true;
    ck::ref<gfx::shared_bitmap> m_bitmap;
  };
};  // namespace ui
