#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysbind.h>
#include <unistd.h>
#include "doomkeys.h"

#include <gfx/font.h>
#include <ui/application.h>
#include <ui/view.h>


extern "C" {
#include "doomgeneric.h"
}

static ui::application main_app;
static ui::window* main_window;
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

 public:
  doomview() {}
  virtual void paint_event(void) override {
    auto s = get_scribe();

    gfx::bitmap b(DOOMGENERIC_RESX, DOOMGENERIC_RESY, DG_ScreenBuffer);

    s.blit(gfx::point(0, 0), b, gfx::rect(0, 0, DOOMGENERIC_RESX, DOOMGENERIC_RESY));

    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override { repaint(); }

  virtual void on_keydown(ui::keydown_event& ev) override { addKeyToQueue(ev.code, ev.c, true); }


  virtual void on_keyup(ui::keyup_event& ev) override { addKeyToQueue(ev.code, ev.c, false); }
};


extern "C" void DG_PumpEventLoop() {
  main_app.drain_messages();
  main_app.dispatch_messages();

  /*
  main_app.eventloop().pump();
  main_app.eventloop().dispatch();
  */
}


extern "C" void DG_Init() {
  memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));

  main_window = main_app.new_window("DOOM", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
  main_window->defer_invalidation(false);


  main_widget = &main_window->set_view<doomview>();
}



extern "C" void DG_DrawFrame() {
  main_widget->repaint();
  DG_PumpEventLoop();
}



extern "C" void DG_SleepMs(uint32_t ms) {
  //
  usleep(ms * 1000);
}


extern "C" uint32_t DG_GetTicksMs() {
  last_tick = current_us() / 1000;
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


extern "C" void DG_SetWindowTitle(const char* title) { printf("set window title to '%s'\n", title); }
