#pragma once

#include <ck/tuple.h>
#include <gfx/bitmap.h>

namespace ui {

  /* fwd decl */
  class view;

  /* A surface is the base class for window-like objects. */
  class surface {
   public:
    virtual ~surface(void) {
    }

    /* get the backing bitmap for this surface, however it's maintained */
    virtual gfx::bitmap *bmp(void) = 0;

    inline int width() {
      return bmp()->width();
    }
    inline int height() {
      return bmp()->height();
    }
    inline void flush(void) {
      invalidate(m_rect);
    }

    /* Tell the surface that a region is invalidated  */
    virtual void invalidate(const gfx::rect &r, bool sync = false) = 0;


    /*
     * Ask the surface to resize, returning a WxH tuple representing
     * the size after the request.
     */
    virtual ck::tuple<int, int> resize(int width, int height) = 0;

    inline virtual ui::view *root_view(void) = 0;


    void reflow();
    // the whole window needs reflowed, so schedule one


    // which view is hovered and focused?
    ui::view *focused = NULL;
    ui::view *hovered = NULL;

   protected:
    virtual void did_reflow(void) = 0;

    gfx::rect m_rect;
  };

};  // namespace ui
