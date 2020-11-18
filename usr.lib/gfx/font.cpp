#include <errno.h>
#include <gfx/font.h>
#include <sys/mman.h>



#define STB_TRUETYPE_IMPLEMENTATION
#include <gfx/stb_truetype.h>

#include FT_BITMAP_H


static bool freetype_initialized = false;
static FT_Library library;

unsigned long tsc(void) {
  uint32_t lo, hi;
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return lo | ((uint64_t)(hi) << 32);
}

namespace gfx {


  font::glyph::glyph(FT_GlyphSlot slot, FT_Face face) {
    FT_Bitmap_Init(&bitmap);

    assert(FT_Bitmap_Copy(library, &slot->bitmap, &bitmap) == 0);
    /* Calculate the advance */
    advance = slot->advance.x >> 6;
    /* Copy the metrics*/
    metrics = slot->metrics;
  }

  font::glyph::~glyph(void) {
    /* Free the bitmap */
    FT_Bitmap_Done(library, &bitmap);
  }



  font::font(ck::unique_ptr<ck::file::mapping> &&d) : data(move(d)) {
    if (!freetype_initialized) {
      FT_Init_FreeType(&library);
      freetype_initialized = true;
    }


    if (FT_New_Memory_Face(library, (const FT_Byte *)data->data(), data->size(), 0, &face)) {
      panic("Could not open font face\n");
    }

    // printf("Font: '%s'  '%s'\n", face->family_name, face->style_name);

    set_line_height(14 /* by default */);
  }

  font::~font(void) {
    /*  */
    FT_Done_Face(face);
  }


  void font::set_line_height(int lg) {
    FT_Set_Pixel_Sizes(face, /* handle to face object */
                       0,    /* pixel_width           */
                       lg);  /* pixel_height          */
  }




  ck::ref<gfx::font> font::get_default(void) {
    static ck::ref<gfx::font> font;
    if (!font) font = gfx::font::get("Lucida Grande");
    return font;
  }


  static bool try_open(ck::file &f, const char *name, const char *ext) {
    char path[100];
    snprintf(path, 100, "/sys/fonts/%s.%s", name, ext);
    return f.open(path, "r");
  }



  static ck::map<ck::string, ck::ref<font>> font_cache;


  //
  ck::ref<font> font::open_absolute(const char *path) {
    ck::file f;
    if (!f.open(path, "r")) {
      return nullptr;
    }
    ck::ref<font> fnt = new font(f.mmap());
    return fnt;
  }

  ck::ref<font> font::get(const char *name) {
    static ck::vec<const char *> exts;
    if (exts.is_empty()) {
      exts.push("ttf");
      exts.push("ttc");
      exts.push("otf");
      exts.push("bdf");
    }

    // First check the font cache (by name)
    if (font_cache.contains(name)) return font_cache.get(name);
    /* Because the font wasn't in the cache, we gotta look it up in our system folder */

    ck::file f;
    /* Try to open the font under some common extensions */
    for (auto *ext : exts) {
      if (try_open(f, name, ext)) {
        // printf("%s found under %s\n", name, ext);
        break;
      }
    }
    /* if the font wasn't found, return null, we don't have it installed */
    if (!f) return nullptr;

    ck::ref<font> fnt = new font(f.mmap());
    font_cache.set(name, fnt);
    return fnt;
  }

  font::glyph *font::load_glyph(uint32_t cp) {
    auto &for_size = gcache[line_height()];

    if (!for_size.contains(cp)) {
      FT_UInt glyph_index = FT_Get_Char_Index(face, cp);
      if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) return NULL;
      auto mode = FT_RENDER_MODE_NORMAL;
      if (FT_Render_Glyph(face->glyph, mode)) return NULL;
      for_size[cp] = ck::make_unique<font::glyph>(face->glyph, face);
      for_size[cp]->mode = mode;
    }
    return for_size.get(cp).get();
  }



  uint32_t font::width(uint32_t cp) {
    auto *gl = load_glyph(cp);
    if (gl == nullptr) return 0;
    return gl->advance;
  }




  /*
   * Draw a single code point at the location dx and dy. dy represents
   * the location of the baseline of the font. In order to draw based on the
   * top line of the font, make sure to add the ascent() to the location.
   */
  int font::draw(int &dx, int &dy, gfx::scribe &s, uint32_t cp, uint32_t fg) {
    font::glyph *gl = NULL;
    {
      // auto start = tsc();
      gl = load_glyph(cp);
      // auto end = tsc();
      // printf("'%c' load took %llu cycles\n", cp, end - start);
    }

    if (gl == NULL) return 0;


    // auto start = tsc();

    int col_start = gl->metrics.horiBearingX >> 6;
    int ascender = gl->metrics.horiBearingY >> 6;

    switch (gl->mode) {
      case FT_RENDER_MODE_NORMAL: {
        for (int y = 0; y < gl->bitmap.rows; y++) {
          int row = y + dy - ascender;
          // int row = rowStartPos + y;
          for (int x = 0; x < gl->bitmap.width; x++) {
            int col = col_start + x + dx;
            unsigned char c = gl->bitmap.buffer[y * gl->bitmap.pitch + x];
            if (c == 0) continue;
            float alpha = c / 255.0;
            s.blend_pixel(col, row, fg, alpha);
          }
        }

        break;
      }

      default: {
        break;
      }
    }


    dx += gl->advance;

    // auto end = tsc();
    // printf("'%c' draw took %llu cycles\n", cp, end - start);

    return 0;
  }

  int font::kerning_for(uint32_t a, uint32_t b) {
    return 0;
    FT_Vector kerning;

    kerning.x = 0;
    kerning.y = 0;

    FT_Get_Kerning(face,               /* handle to face object */
                   a,                  /* left glyph index      */
                   b,                  /* right glyph index     */
                   FT_KERNING_DEFAULT, /* kerning mode          */
                   &kerning);          /* target vector         */
    return kerning.x;
  }


  int font::ascent(void) { return face->size->metrics.ascender >> 6; }
  int font::descent(void) { return face->size->metrics.descender >> 6; }
  int font::line_height(void) { return face->size->metrics.height >> 6; }

};  // namespace gfx
