#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <unistd.h>
#include "doomkeys.h"
#include "ui/textalign.h"

#include <gfx/font.h>
#include <ui/application.h>
#include <ui/view.h>
#include <ui/boxlayout.h>
#include <ck/timer.h>
#include <ck/time.h>

extern "C" {
#include "doomgeneric.h"
}


class doomview;


#define SCALE 3

class doom_window : public ui::window {
 public:
  doom_window(void) : ui::window("DOOM", DOOMGENERIC_RESX * SCALE, DOOMGENERIC_RESY * SCALE) {
    // set_theme(0x353535, 0xFFFFFF, 0x353535);
    // compositor_sync(true);
  }

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    return ui::make<ui::hbox>({ui::make<doomview>()});

    // clang-format on
  }
};



static ui::application main_app;
static doom_window* main_window;
static ui::view* main_widget;
static size_t last_tick = 0;

ck::ref<gfx::font> doomuifont;

#define KEYQUEUE_SIZE 128


static inline auto current_us(void) { return sysbind_gettime_microsecond(); }

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(int code, char c) {
#define BIND(from, to)  \
  if (code == (from)) { \
    return (to);        \
  }

  BIND(key_return, KEY_ENTER);
  BIND(key_escape, KEY_ESCAPE);
  BIND(key_tab, KEY_TAB);

  // use is e, fire is space
  BIND(key_e, KEY_USE);
  BIND(key_space, KEY_FIRE);


  // WASD key bindings
  BIND(key_w, KEY_UPARROW);
  BIND(key_a, KEY_STRAFE_L);
  BIND(key_s, KEY_DOWNARROW);
  BIND(key_d, KEY_STRAFE_R);



  BIND(key_left, KEY_LEFTARROW);
  BIND(key_right, KEY_RIGHTARROW);
  BIND(key_up, KEY_UPARROW);
  BIND(key_down, KEY_DOWNARROW);

  BIND(key_leftshift, KEY_RSHIFT);
  BIND(key_rightshift, KEY_RSHIFT);

  BIND(key_control, KEY_RCTRL);
  BIND(key_alt, KEY_RALT);
#undef BIND

  return tolower(c);
}



static void addKeyToQueue(int code, char c, bool pressed) {
  unsigned char key = convertToDoomKey(code, c);
  unsigned short keyData = (pressed << 8) | key;


  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

class doomview : public ui::view {
  long frames = 0;
  long start_time = current_us();
  float v = 0.0;

  float scale = 1.0;

 public:
  doomview() {
    main_widget = this;
    set_font("Source Code Pro");
  }


  virtual void mouse_event(ui::mouse_event& ev) override {
    if (ev.ds != 0) {
      scale *= 1.0 + (ev.ds * 0.01);
      scale = fmax(0.1, scale);
      surface()->resize((DOOMGENERIC_RESX * SCALE) * scale, (DOOMGENERIC_RESY * SCALE) * scale);
    }
  }


  virtual void paint_event(gfx::scribe& s) override {
    gfx::bitmap b(DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer);


    auto font = get_font();
    assert(font);

    font->with_line_height(12, [&] {
      unsigned long long nfree;
      unsigned long long total;

      gfx::scribe s = gfx::scribe(b);

      sysbind_getraminfo(&nfree, &total);
      auto label = ck::string::format("Memory: %3.4f%%", ((float)nfree / (float)total) * 100.0f);
      gfx::rect r = rect();
      s.draw_text(*font, r, label, ui::TextAlign::TopLeft, 0xFFFFFF, true);
    });



    gfx::rect r = gfx::rect(0, 0, width(), height());
    {
      // ck::time::logger l("doom blit and effect");

      s.blit_scaled(b, r);

      // s.saturation(sin(ck::time::ms() / 1000.0), r);
      // s.sepia((sin(ck::time::ms() / 1000.0) + 1) / 2.0, r);
      // s.stackblur(10, r);
      // s.noise(0.05, r);
    }
  }

  virtual void on_keydown(ui::keydown_event& ev) override { addKeyToQueue(ev.code, ev.c, true); }
  virtual void on_keyup(ui::keyup_event& ev) override { addKeyToQueue(ev.code, ev.c, false); }
};


extern "C" void DG_PumpEventLoop() {
  main_app.eventloop().run_deferred();
  main_app.eventloop().check_timers();

  main_app.drain_messages();
  main_app.dispatch_messages();
}




extern "C" void DG_Init() {
  if (!main_app.connected()) {
    printf("No connection to the window server.\n");
    exit(EXIT_FAILURE);
  }
  memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

  main_window = new doom_window();

	exit(-1);
}



extern "C" void DG_DrawFrame() {
  if (main_widget != NULL) {
    main_widget->update();
  }
  DG_PumpEventLoop();
}



extern "C" void DG_SleepMs(uint32_t ms) {
  //
  usleep(ms * 1000);
}


extern "C" uint32_t DG_GetTicksMs() {
  auto this_tick = current_us() / 1000;
  if (this_tick < last_tick) {
    printf("time travel! %llu -> %llu = -%llu\n", last_tick, this_tick, last_tick - this_tick);
    exit(EXIT_FAILURE);
  } else {
    last_tick = this_tick;
  }
  return last_tick;
}


extern "C" int DG_GetKey(int* pressed, unsigned char* doomKey) {
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
    // key queue is empty

    return 0;
  } else {
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueue[s_KeyQueueReadIndex] = 0;
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }
}


extern "C" void DG_SetWindowTitle(const char* title) {
  printf("set window title to '%s'\n", title);
}
