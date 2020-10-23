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



  font::font(ck::unique_ptr<ck::file::mapping> &&d, int lh) : data(move(d)) {
    if (!freetype_initialized) {
      FT_Init_FreeType(&library);
      freetype_initialized = true;
    }


    if (FT_New_Memory_Face(library, (const FT_Byte *)data->data(), data->size(), 0, &face)) {
      panic("Could not open font face\n");
    }

    set_line_height(lh);
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
    if (!font) font = gfx::font::open("Helvetica", 14);
    return font;
  }



  ck::ref<font> font::open(const char *name, int lh) {
    char path[100];
    snprintf(path, 100, "/usr/res/fonts/%s.ttf", name);

    ck::file f;
    if (!f.open(path, "r")) {
      return nullptr;
    }

    return new font(f.mmap(), lh);
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
