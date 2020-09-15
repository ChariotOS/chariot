#include <chariot/fs/magicfd.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <ui/application.h>


#define SSFN_IMPLEMENTATION /* use the normal renderer implementation */
#include "./ssfn.h"


struct line {
  gfx::point start;
  gfx::point end;
};

ck::unique_ptr<ck::file::mapping> font_mapping;




class painter : public ui::view {
  int x, y;
  ssfn_t ctx;     /* the renderer context */
  ssfn_buf_t buf; /* the destination pixel buffer */

  ck::unique_ptr<ck::file::mapping> mapping;

 public:
  painter(ck::file& f) {
    mapping = f.mmap();

    /* you don't need to initialize the library, just make sure the context is zerod out */
    memset(&ctx, 0, sizeof(ssfn_t));

    /* add one or more fonts to the context. Fonts must be already in memory */
    ssfn_load(&ctx, font_mapping->data()); /* you can add different styles... */
  }

  ~painter(void) {
    /* free resources */
    ssfn_free(&ctx); /* free the renderer context's internal buffers */
  }


  virtual void paint_event(void) override {
    auto s = get_scribe();

    // s.clear(0xFF'FF'FF);



    auto& bmp = window()->bmp();


    int size = 12;
    /* select the typeface to use */
    ssfn_select(&ctx, SSFN_FAMILY_SANS, NULL, /* family */
                SSFN_STYLE_REGULAR,           /* style */
                size                          /* size */
    );


    /* describe the destination buffer. Could be a 32 bit linear framebuffer as well */
    buf.ptr = (unsigned char*)bmp.pixels(); /* address of the buffer */
    buf.w = bmp.width();                    /* width */
    buf.h = bmp.height();                   /* height */
    buf.p = buf.w * sizeof(int);            /* bytes per line */
    buf.fg = 0xFF000000;                    /* foreground color */

    buf.x = 0;
    buf.y = size;

    auto* data = mapping->as<const char>();

    for (size_t i = 0; i < mapping->size(); i++) {
      char c = data[i];
      char text[2];
      text[0] = c;
      text[1] = 0;

      if (c == '\n') {
        buf.x = 0;
        buf.y += size;
        continue;
      }

      ssfn_render(&ctx, &buf, (char*)text);
      if (buf.x > buf.w) {
        buf.x = 0;
        buf.y += size;
      }
    }

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override {
    x = ev.x;
    y = ev.y;
    repaint();
  }
};


#include <setjmp.h>
#include <signal.h>



int main(int argc, char** argv) {
  int* a = NULL;
  *a = 10;



  ck::file fnt;
  fnt.open("/usr/res/fonts/Vera.sfn", "r");
  font_mapping = fnt.mmap();



  ck::file text;
  text.open("/usr/res/misc/lorem.txt", "r");

  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("My Window", 640, 480);
  win->set_view<painter>(text);

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();


  return 0;
}
