#pragma once

#include <ck/object.h>
#include <ck/string.h>
#include <ck/tuple.h>
#include <gfx/bitmap.h>
#include <gfx/font.h>
#include <gfx/rect.h>
#include <lumen/msg.h>
#include <ui/color.h>
#include <ui/surface.h>
#include <ui/view.h>

namespace ui {

  // fwd decl
  class appication;



  class windowframe : public ui::view {
    ck::ref<gfx::font> m_frame_font;
    ck::ref<gfx::font> m_icon_font;

   public:
    void set_theme(uint32_t bg, uint32_t fg, uint32_t border);
    static constexpr uint32_t FRAME_COLOR = 0xe7ebee;
    static constexpr int TITLE_HEIGHT = 29;
    static constexpr int PADDING = 0;
    windowframe(void);
    virtual ~windowframe(void);
    virtual void paint_event(void) override;
    virtual void mouse_event(ui::mouse_event &) override;
  };


  class window : public ui::surface {
    friend windowframe;

   public:
    window(int id, ck::string name, gfx::rect r, ck::ref<gfx::shared_bitmap>);
    ~window();
    inline const ck::string &name(void) {
      return m_name;
    }

    void handle_input(struct lumen::input_msg &);

    template <typename T, typename... Args>
    inline T &set_view(Args &&...args) {
      ui::view *v = new T(forward<Args>(args)...);
      m_frame->clear();
      m_frame->add(v);
      v->set_focused();
      return *(T *)v;
    }

    inline void defer_invalidation(bool d) {
      m_defer_invalidation = d;
    }
    inline void set_theme(uint32_t bg, uint32_t fg, uint32_t border) {
      m_frame->set_theme(bg, fg, border);
    }
    void schedule_reflow();

    virtual ck::tuple<int, int> resize(int width, int height);
    virtual void invalidate(const gfx::rect &r, bool sync = false);
    virtual inline gfx::bitmap *bmp(void) {
      return m_bitmap.get();
    }
    virtual ui::view *root_view(void);


    inline int id(void) {
      return m_id;
    }

   protected:
    virtual void did_reflow(void);

    ck::unique_ptr<ui::windowframe> m_frame;

    bool m_pending_reflow = false;
    ck::vec<gfx::rect> m_pending_invalidations;

    bool m_light_theme = true;
    int m_id;
    ck::string m_name;

    bool m_defer_invalidation = true;
    ck::ref<gfx::shared_bitmap> m_bitmap;


    /* used for mouse tracking */
    int mouse_down = 0; /* Current mouse press state */

    /* The last location the mouse was hovered at */
    gfx::point last_hover;
    /* The last locations the mouse was clicked at */
    gfx::point last_lclick; /* Left click */
    gfx::point last_rclick; /* Right click */
  };
};  // namespace ui
