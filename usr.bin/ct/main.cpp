#include <chariot/fs/magicfd.h>
#include <ck/tuple.h>
#include <cxxabi.h>
#include <fcntl.h>
#include <gfx/font.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/sysbind.h>
#include <ui/application.h>
#include <typeinfo>


#define SSFN_IMPLEMENTATION /* use the normal renderer implementation */
#include "./ssfn.h"


struct line {
  gfx::point start;
  gfx::point end;
};

ck::unique_ptr<ck::file::mapping> font_mapping;


auto title_font(void) {
  static auto fnt = gfx::font::open("chicago-normal", 12);
  return fnt;
}

// #define USE_SFN


class painter : public ui::view {
  int x, y;
#ifdef USE_SFN
  ssfn_t ctx;     /* the renderer context */
  ssfn_buf_t buf; /* the destination pixel buffer */
#endif

  ck::unique_ptr<ck::file::mapping> mapping;

 public:
  painter(ck::file& f) {
    mapping = f.mmap();

#ifdef USE_SFN
    /* you don't need to initialize the library, just make sure the context is zerod out */
    memset(&ctx, 0, sizeof(ssfn_t));

    /* add one or more fonts to the context. Fonts must be already in memory */
    ssfn_load(&ctx, font_mapping->data()); /* you can add different styles... */
#endif
  }

  ~painter(void) {
#ifdef USE_SFN
    /* free resources */
    ssfn_free(&ctx); /* free the renderer context's internal buffers */
#endif
  }


  virtual void paint_event(void) override {
    auto s = get_scribe();

    // s.clear(0xFF'FF'FF);



    auto& bmp = window()->bmp();


#ifndef USE_SFN
    auto pr = gfx::printer(s, *title_font(), 0, 0, bmp.width());
    pr.set_color(0x000000);
    auto* data = mapping->as<const char>();

    for (size_t i = 0; i < mapping->size(); i++) {
      char c = data[i];
      pr.printf("%c", c);
    }
#else
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
#endif

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override {
    x = ev.x;
    y = ev.y;
    repaint();
  }
};


template <typename T>
ck::string type_name(const T& val) {
  size_t len = 512;
  char buffer[len];
  int status = 0;
  char* name = __cxxabiv1::__cxa_demangle(typeid(val).name(), buffer, &len, &status);
  return ck::string(name, len);
}


// #include "../../usr.lib/sqlite/src/sqlite3.h"


/*
class sqlite3_db {
  struct sqlite3* db = NULL;

 public:
  sqlite3_db() {}

  ~sqlite3_db() {
    if (db != NULL) sqlite3_close(db);
  }

  int open(const char* path) { return sqlite3_open(path, &db); }

  auto exec(const char* query) {
    ck::vec<ck::string> rows;

    char* err = NULL;

    sqlite3_exec(
        db, query,
        [](void* arg, int argc, char** argv, char** azColName) -> int {
          int i;
          for (i = 0; i < argc; i++) {
            printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
          }
          printf("\n");
          return 0;
        },
        NULL, &err);
		if (err != NULL) {
			printf("err: %s\n", err);
			sqlite3_free((void*)err);
		}

    return rows;
  }
};
*/


#include <GL/osmesa.h>

int main(int argc, char** argv) {


	// create and destroy the gl context as a test
	OSMesaContext gl_ctx = OSMesaCreateContext(OSMESA_ARGB, NULL);

	printf("Created!\n");
	OSMesaDestroyContext(gl_ctx);

	return 0;

#if 0

	for (int i = 0; true; i++) {
		char buf[100];
		snprintf(buf, 100, "echo %d", i);
		system(buf);
	}
	return 0;
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
#endif
}
