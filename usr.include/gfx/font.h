#pragma once

#include <ck/io.h>
#include <ck/map.h>
#include <ck/ptr.h>
#include <gfx/scribe.h>
#include <stdint.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <ck/lock.h>

// #define GFX_FNT_MAGIC 0x464f4e54  // 'FONT'

namespace gfx {


  class font : public ck::refcounted<font> {
   protected:
    font(ck::unique_ptr<ck::file::mapping> &&);
    ck::unique_ptr<ck::file::mapping> data;

    FT_Face face; /* handle to face object */

    struct glyph {
      /* The backing freetype bitmap (a copy) */
      FT_Bitmap bitmap;
      /* Metrics about  */
      FT_Glyph_Metrics metrics;
      /* What mode the bitmap was rendered in */
      FT_Render_Mode mode;
      /* The x-advance in pixels */
      int advance;
      /* Load from a glyph slot */
      glyph(FT_GlyphSlot, FT_Face face);
      ~glyph(void);
    };

    /*
     * We cache glyphs becasue asking freetype to re-render
     * each time is really inefficient
     */
    ck::map<int, ck::map<uint32_t, ck::unique_ptr<font::glyph>>> gcache;

    /* Load a glyph for the current codepoint, returning null if it fails */
    font::glyph *load_glyph(uint32_t cp);


    /* So the printer class can access all this info */
    friend gfx::printer;
    ck::mutex m_lock;


   public:
    ~font(void);

    int draw(int &x, int &y, gfx::scribe &, uint32_t cp, uint32_t color);

    enum FontStyle { Variable, Monospace };

    // public interface
    static ck::ref<font> get(const char *name);
    static ck::ref<font> get_default(void);
    static ck::ref<font> open_absolute(const char *path);



    int line_height(void);
    // distance from top to baseline
    int ascent(void);
    // distance from bottom to baseline
    int descent(void);

    int kerning_for(uint32_t cur, uint32_t next);

    void set_line_height(int new_height);


    template <typename F>
    void with_line_height(int lh, F f) {
      lock();
      set_line_height(lh);
      f();
      unlock();
    }


    // total width of a codepoint from the left edge
    uint32_t width(uint32_t cp);

    inline uint32_t width(ck::string_view v) {
      uint32_t w = 0;
      for (char c : v) w += width(c);
      return w;
    }


		inline auto &ft_face() { return this->face; }

    /* whenever you use the font, you must lock it. The font will not lock itself */
    inline void lock(void) { m_lock.lock(); }
    inline void unlock(void) { m_lock.unlock(); }
  };

}  // namespace gfx
