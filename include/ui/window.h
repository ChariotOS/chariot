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
#include "gfx/disjoint_rects.h"

namespace ui {

  // fwd decl
  class appication;



  class windowframe : public ui::view {
    ck::ref<gfx::font> m_frame_font;
    ck::ref<gfx::font> m_icon_font;

   public:
    void set_theme(uint32_t bg, uint32_t fg, uint32_t border);
    static constexpr int TITLE_HEIGHT = 24;
    static constexpr int PADDING = 0;
    windowframe(void);
    virtual ~windowframe(void);
    virtual void paint_event(gfx::scribe &) override;
    virtual void mouse_event(ui::mouse_event &) override;
    virtual void custom_layout(void) override;
  };


  class window : public ui::surface {
    friend windowframe;

   public:
    window(ck::string name, int w, int h, int flags = 0);
    virtual ~window();


   public:
    inline const ck::string &name(void) { return m_name; }
    void handle_input(struct lumen::input_msg &);

    // build the certain window you are implementing.
    virtual ck::ref<ui::view> build(void) { return make<ui::view>(); }
    void rebuild(void);

    inline void compositor_sync(bool d) { m_compositor_sync = d; }
    inline void set_theme(uint32_t bg, uint32_t fg, uint32_t border) {
      m_frame->set_theme(bg, fg, border);
    }

    virtual ck::pair<int, int> resize(int width, int height);
    virtual void invalidate(const gfx::rect &r);
    virtual inline gfx::bitmap *bmp(void) {
      return double_buffer() ? m_private_bitmap.get() : m_shared_bitmap.get();
    }
    virtual ui::view *root_view(void);


    inline int id(void) { return m_id; }

    bool double_buffer(void) const { return m_double_buffer; }
    void set_double_buffer(bool);

   protected:
    inline void set_view(ck::ref<ui::view> v) {
      m_frame->clear();
      m_frame->add(v);
      v->set_focused();
    }

    void update_private_buffer(void);
    void actually_do_invalidations(void);
    virtual void did_reflow(void);

    ck::ref<ui::windowframe> m_frame;

    bool m_pending_reflow = false;
    gfx::disjoint_rects m_pending_invalidations;

    bool m_light_theme = true;
    int m_id;
    ck::string m_name;

    // software "vsync" with the compositor
    bool m_compositor_sync = false;
    ck::ref<gfx::bitmap> m_private_bitmap;
    ck::ref<gfx::shared_bitmap> m_shared_bitmap;

    // by default, double buffer
    bool m_double_buffer = true;

    /* used for mouse tracking */
    int mouse_down = 0; /* Current mouse press state */

    /* The last location the mouse was hovered at */
    gfx::point last_hover;
    /* The last locations the mouse was clicked at */
    gfx::point last_lclick; /* Left click */
    gfx::point last_rclick; /* Right click */
  };


  template <typename T>
  class simple_window : public ui::window {
   public:
    simple_window(ck::string name, int w, int h, int flags = 0) : window(move(name), w, h, flags) {}
    virtual ck::ref<ui::view> build(void) override { return ui::make<T>(); }
  };


};  // namespace ui
