#include <ui/application.h>
// #include "terminalview.h"
#include <ck/dir.h>
#include <gfx/color.h>
#include <gfx/font.h>
#include <gfx/scribe.h>

extern const char **environ;

#include <math.h>

#include <ft2build.h>
#include FT_FREETYPE_H



struct terminalview : public ui::view {
  int rows, cols;

  int cw, ch;  // char width and height

  int mouse_x = 0;
  int mouse_y = 0;

  ck::file font_file;
  ck::unique_ptr<ck::file::mapping> file_mapping;


  FT_Library library; /* handle to library     */
  FT_Face face;       /* handle to face object */

 public:
  terminalview(int rows, int cols) {
    this->rows = rows;
    this->cols = cols;


    font_file.open("/usr/res/fonts/JetBrainsMono-Medium.ttf", "r");
    file_mapping = font_file.mmap();
    // stbtt_InitFont(&info, file_mapping->as<const unsigned char>(), 0);


    if (FT_Init_FreeType(&library)) {
      panic("could not init freetype\n");
    }

    if (FT_New_Memory_Face(library, (const FT_Byte *)file_mapping->data(), file_mapping->size(), 0, &face)) {
      panic("Could not open font face\n");
    }
  }




  virtual void mounted(void) override {}

  virtual void on_mouse_move(ui::mouse_event &ev) override {
    mouse_x = ev.x;
    mouse_y = ev.y;
    repaint();
  }



  virtual void paint_event(void) override {
    auto s = get_scribe();

    // split the foreground up
    uint32_t fg = 0xFFFFFF;
    uint32_t bg = 0x000000;

    // uint32_t fg = 0x00FF00;
    // uint32_t bg = 0xFFFFFF;
    s.clear(bg);


    FT_GlyphSlot slot = face->glyph; /* a small shortcut */
    FT_UInt glyph_index;

    FT_Set_Pixel_Sizes(face,     /* handle to face object */
                       0,        /* pixel_width           */
                       mouse_y); /* pixel_height          */


    char text[50];
    snprintf(text, 50, "Random value: %08x", rand());

    int pen_x = this->mouse_x;
    int pen_y = this->mouse_y;

    auto mode = FT_RENDER_MODE_NORMAL;


    for (int n = 0; n < strlen(text); n++) {
      FT_UInt glyph_index;

      /* retrieve glyph index from character code */
      glyph_index = FT_Get_Char_Index(face, text[n]);

      /* load glyph image into the slot (erase previous one) */
      if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT)) continue; /* ignore errors */

      /* convert to an anti-aliased bitmap */
      if (FT_Render_Glyph(face->glyph, mode)) continue;

      int col_start = slot->metrics.horiBearingX >> 6;
      int ascender = slot->metrics.horiBearingY >> 6;


      // int w = slot->metrics.width >> 6;
      // s.draw_hline(col_start + pen_x, pen_y - (face->size->metrics.ascender >> 6), w, 0xFF0000);
      // s.draw_hline(col_start + pen_x, pen_y - (face->size->metrics.descender >> 6), w, 0xFF0000);




      // LCD render mode
      if (mode == FT_RENDER_MODE_LCD) {
        float fg_r, fg_g, fg_b;
        float bg_r, bg_g, bg_b;

        auto split_into = [](uint32_t color, float &r, float &g, float &b) {
          r = ((color >> 16) & 0xFF) / 255.0;
          g = ((color >> 8) & 0xFF) / 255.0;
          b = ((color >> 0) & 0xFF) / 255.0;
        };

        split_into(fg, fg_r, fg_g, fg_b);
        split_into(bg, bg_r, bg_g, bg_b);

        for (int y = 0; y < slot->bitmap.rows; y++) {
          int row = y + pen_y - ascender;
          // int row = rowStartPos + y;
          for (int x = 0; x < slot->bitmap.width / 3; x++) {
            int col = col_start + x + pen_x;
            // interpret it as bgr (cause it looks nice on my monitor)
            float b_alpha = slot->bitmap.buffer[y * slot->bitmap.pitch + x * 3] / 255.0;
            float g_alpha = slot->bitmap.buffer[y * slot->bitmap.pitch + x * 3 + 1] / 255.0;
            float r_alpha = slot->bitmap.buffer[y * slot->bitmap.pitch + x * 3 + 2] / 255.0;

            if (r_alpha == 0 && g_alpha == 0 && b_alpha == 0) continue;

            // split the background color up
            uint32_t new_bg = s.get_pixel(col, row);
            if (new_bg != bg) {
              bg = new_bg;
              split_into(bg, bg_r, bg_g, bg_b);
            }


            // The blending function for placing text over a background is
            //    dst = alpha * src + (1 - alpha) * dst
            float r = (fg_r * r_alpha) + (bg_r * (1 - r_alpha));
            float g = (fg_g * g_alpha) + (bg_g * (1 - g_alpha));
            float b = (fg_b * b_alpha) + (bg_b * (1 - b_alpha));
            s.draw_pixel(col, row, gfx::color::rgb(r * 255, g * 255, b * 255));
          }
        }
      }


      // Normal rendering mode
      if (mode == FT_RENDER_MODE_NORMAL) {
        for (int y = 0; y < slot->bitmap.rows; y++) {
          int row = y + pen_y - ascender;
          // int row = rowStartPos + y;
          for (int x = 0; x < slot->bitmap.width; x++) {
            int col = col_start + x + pen_x;
            float alpha = slot->bitmap.buffer[y * slot->bitmap.pitch + x] / 255.0;
            s.blend_pixel(col, row, fg, alpha);
          }
        }
      }

      pen_x += slot->advance.x >> 6;
    }
  }
};



int main() {
  ui::application app;

  ui::window *win = app.new_window("Terminal", 640, 480);

  int cols = 80;
  int rows = 24;
  win->set_view<terminalview>(rows, cols);

  app.start();
  return 0;

#if 0
  const char *cmd[] = {"/bin/echo", "hello, world", NULL};
  int ret = sysbind_execve(cmd[0], cmd, environ);
  perror("exec");
  exit(ret);

  int ptmxfd = open("/dev/ptmx", O_RDWR | O_CLOEXEC);

  ck::eventloop ev;
  int pid = sysbind_fork();
  if (pid == 0) {
    ck::file pts;
    pts.open(ptsname(ptmxfd), "r+");

    while (1) {
      char data[32];
      auto n = pts.read(data, 32);
      if (n < 0) continue;
      ck::hexdump(data, n);
    }

    ev.start();
  } else {
    ck::file ptmx(ptmxfd);

    // send input
    ck::in.on_read(fn() {
      int c = getchar();
      if (c == EOF) return;
      ptmx.write(&c, 1);
    });


    // echo
    ptmx.on_read(fn() {
      char buf[512];
      auto n = ptmx.read(buf, 512);
      if (n < 0) {
        perror("ptmx read");
        return;
      }
      ck::out.write(buf, n);
    });

    ev.start();
  }
#endif
  return 0;
}
