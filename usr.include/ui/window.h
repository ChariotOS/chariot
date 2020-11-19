#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <ck/tuple.h>
#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <lumen/msg.h>
#include <ui/color.h>
#include <ui/view.h>

namespace ui {

  // fwd decl
  class appication;



  class windowframe : public ui::stackview {
    ck::ref<gfx::font> m_frame_font;
  	ck::ref<gfx::font> m_icon_font;

   public:

		void set_theme(uint32_t bg, uint32_t fg, uint32_t border);
    static constexpr uint32_t FRAME_COLOR = 0xe7ebee;
    static constexpr int TITLE_HEIGHT = 29;
    static constexpr int PADDING = 1;
    windowframe(void);
    virtual ~windowframe(void);
    virtual void paint_event(void) override;
  };


  class window : public ck::object {
    CK_OBJECT(ui::window);



		friend windowframe;
   public:
    window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap>);
    ~window();
    void flush(void);

    inline int width() { return bmp().width(); }
    inline int height() { return bmp().height(); }

    void invalidate(const gfx::rect &r, bool sync = false);

    inline gfx::shared_bitmap &bmp(void) { return *m_bitmap; }


    inline const ck::string &name(void) { return m_name; }

    void handle_input(struct lumen::input_msg &);


    template <typename T, typename... Args>
    inline T &set_view(Args &&... args) {
      // m_main_view = NULL;

      ui::view *v = new T(forward<Args>(args)...);

      m_frame->clear();
      m_frame->add(v);

      v->set_focused();

      return *(T *)v;
    }

    inline void defer_invalidation(bool d) { m_defer_invalidation = d; }


    // the whole window needs reflowed, so schedule one
    void schedule_reflow();
    ck::tuple<int, int> resize(int width, int height);


    // which view is hovered and focused?
    ui::view *focused = NULL;
    ui::view *hovered = NULL;


		inline void set_theme(uint32_t bg, uint32_t fg, uint32_t border) { m_frame->set_theme(bg, fg, border); }


   protected:
    bool m_pending_reflow = false;

    ck::unique_ptr<ui::windowframe> m_frame;

    ck::vec<gfx::rect> m_pending_invalidations;


		bool m_light_theme = true;
    long m_id;
    ck::string m_name;
    gfx::rect m_rect;

    bool m_defer_invalidation = true;
    ck::ref<gfx::shared_bitmap> m_bitmap;
  };
};  // namespace ui
