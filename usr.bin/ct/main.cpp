#include <chariot/fs/magicfd.h>
#include <ck/rand.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <ui/application.h>
#include <sys/mman.h>

#define SSFN_IMPLEMENTATION /* use the normal renderer implementation */
#include "./ssfn.h"


/*
struct line {
  gfx::point start;
  gfx::point end;
};

ck::unique_ptr<ck::file::mapping> font_mapping;
*/


#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"



class painter : public ui::view {
  // int x, y;
  // ssfn_t ctx;     /* the renderer context */
  // ssfn_buf_t buf; /* the destination pixel buffer */
  // ck::unique_ptr<ck::file::mapping> mapping;

  stbtt_fontinfo& font;

 public:
  painter(stbtt_fontinfo& font) : font(font) {}



  virtual void paint_event(void) override {
    auto s = get_scribe();

    s.clear(0xFFFFFF);


    int ascent = 0, baseline = 0;
    float scale;  // leave a little padding in case the character extends left

    int em = 13;
    scale = stbtt_ScaleForPixelHeight(&font, em);
    stbtt_GetFontVMetrics(&font, &ascent, 0, 0);
    baseline = (int)(ascent * scale);

    const char* text = "the quick brown fox";

    int x = 0;
    for (int i = 0; text[i] != 0; i++) {
      char c = text[i];


      if (c != ' ') {
        int w = 0;
        int h = 0;
        int xoff, yoff;
        unsigned char* gl = stbtt_GetCodepointBitmapSubpixel(&font, scale, scale, 0, 0, c, &w, &h, &xoff, &yoff);
        printf("%c %3d %3d\n", c, xoff, yoff);
        for (int oy = 0; oy < h; oy++) {
          for (int ox = 0; ox < w; ox++) {
            int p = 255 - gl[oy * w + ox];
            s.draw_pixel(x + ox, baseline + yoff + oy, p << 0 | p << 8 | p << 16);
          }
        }
        int kern = stbtt_GetCodepointKernAdvance(&font, text[i], text[i + 1]);
        x += roundf(kern * scale) + w;

        free(gl);
      } else {
        x += em / 3;
      }
    }



    invalidate();
  }
};





int main(int argc, char** argv) {


  stbtt_fontinfo font;

  ck::file f;

  f.open("/usr/res/fonts/arialce.ttf", "rb");

  auto mapping = f.mmap();
  stbtt_InitFont(&font, (const unsigned char*)mapping->data(), 0);

  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("My Window", 600, 300);
  win->set_view<painter>(font);

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
