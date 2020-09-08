#include <chariot/fs/magicfd.h>
#include <ck/timer.h>
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


#include <GL/glu.h>
#include <GL/osmesa.h>



class glpainter : public ui::view {
  OSMesaContext om;
  bool initialized = false;
  float xrot = 100.0f;
  float yrot = -100.0f;

  // float xdiff = 100.0f;
  // float ydiff = 100.0f;

  float tra_x = 0.0f;
  float tra_y = 0.0f;
  float tra_z = 0.0f;


  float grow_shrink = 70.0f;
  float resize_f = 1.0f;


  ck::ref<ck::timer> compose_timer;

 public:
  glpainter(void) { om = OSMesaCreateContext(OSMESA_BGRA, NULL); }

  ~glpainter(void) { OSMesaDestroyContext(om); }


  void reshape(int w, int h) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glViewport(0, 0, w, h);

    gluPerspective(grow_shrink, resize_f * w / h, resize_f, 100 * resize_f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
  }



  int initgl(void) {
    if (initialized == true) return 0;

    compose_timer = ck::timer::make_interval(1000 / 100, [this] { this->tick(); });
    initialized = true;
    auto* win = window();
    auto& bmp = win->bmp();

    if (!OSMesaMakeCurrent(om, bmp.pixels(), GL_UNSIGNED_BYTE, bmp.width(), bmp.height())) return 1;

    OSMesaPixelStore(OSMESA_Y_UP, 0);

    reshape(bmp.width(), bmp.height());

    // glShadeModel(GL_SMOOTH); // THIS IS SLOW
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);  // black background

    glClearDepth(1.0f);       // Depth Buffer Setup
    glEnable(GL_DEPTH_TEST);  // Enables Depth Testing
    glDepthFunc(GL_LEQUAL);   // The Type Of Depth Test To Do
    // glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);  // Really Nice Perspective Calculations
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

    glEnable(GL_MULTISAMPLE_ARB);  // enable MSAA



    return 0;
  }




  void tick() {
    xrot += 0.3f;
    yrot += 0.4f;
    repaint();
  }

  virtual void paint_event(void) override {
    // initgl();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();

    gluLookAt(0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    glRotatef(xrot, 1.0f, 0.0f, 0.0f);
    glRotatef(yrot, 0.0f, 1.0f, 0.0f);

    drawBox();

    glFlush();
    invalidate();
  }

  void drawBox() {
    glTranslatef(tra_x, tra_y, tra_z);

    glBegin(GL_QUADS);

    glColor3f(1.0f, 0.0f, 0.0f);
    // FRONT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    // BACK
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);

    glColor3f(0.0f, 1.0f, 0.0f);
    // LEFT
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    // RIGHT
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, 0.5f);

    glColor3f(0.0f, 0.0f, 1.0f);
    // TOP
    glVertex3f(-0.5f, 0.5f, 0.5f);
    glVertex3f(0.5f, 0.5f, 0.5f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, 0.5f, -0.5f);
    glVertex3f(-0.5f, 0.5f, -0.5f);
    glColor3f(1.0f, 0.0f, 0.0f);
    // BOTTOM
    glVertex3f(-0.5f, -0.5f, 0.5f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, -0.5f);
    glVertex3f(0.5f, -0.5f, 0.5f);
    glEnd();  // GL_QUADS
  }
};

int main(int argc, char** argv) {
  // connect to the window server
  ui::application app;

  ui::window* win = app.new_window("OpenGL Test (Mesa)", 640, 640);
  auto& vw = win->set_view<glpainter>();
  vw.initgl();

  auto input = ck::file::unowned(0);
  input->on_read([&] {
    getchar();
    ck::eventloop::exit();
  });

  // start the application!
  app.start();




  return 0;
}
