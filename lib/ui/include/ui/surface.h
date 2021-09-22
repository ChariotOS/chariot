#pragma once

#include <ck/pair.h>
#include <gfx/bitmap.h>
#include <ck/timer.h>
#include <ck/ptr.h>

namespace ui {

  /* fwd decl */
  class view;

  /* A surface is the base class for window-like objects. */
  class surface : public ck::weakable<ui::surface> {
   public:
    surface();
    virtual ~surface(void) {}

    virtual gfx::bitmap *bmp(void) = 0;
    virtual void invalidate(const gfx::rect &r) = 0;
    virtual ck::pair<int, int> resize(int width, int height) = 0;
    virtual ui::view *root_view(void) = 0;
    virtual void did_reflow(void) = 0;


    inline int width() { return bmp()->width(); }
    inline int height() { return bmp()->height(); }
    inline void flush(void) { invalidate(m_rect); }

    // the whole window needs reflowed, so schedule one
    void schedule_reflow();

    // which view is hovered and focused?
    ui::view *focused = NULL;
    ui::view *hovered = NULL;

    // ask that a ui::animation_event be sent at the next possible moment
    void request_animation_frame(ck::weak_ref<ui::view> view);

   protected:
    void do_reflow();
    bool m_pending_reflow = false;
    gfx::rect m_rect;
    ck::timer m_animation_timer;
    ck::vec<ck::weak_ref<ui::view>> m_views_wanting_animation;
  };

};  // namespace ui
