#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "doomkeys.h"


#include <ui/application.h>
#include <ui/view.h>


extern "C" {
#include "doomgeneric.h"
}

static ui::application main_app;
static ui::window* main_window;
static ui::view* main_widget;



#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(int code) {
  unsigned char key = 0;
  switch (code) {
    case key_return:
      key = KEY_ENTER;
      break;
    case key_escape:
      key = KEY_ESCAPE;
      break;
		case key_a:
    case key_left:
      key = KEY_LEFTARROW;
      break;
		case key_d:
    case key_right:
      key = KEY_RIGHTARROW;
      break;
    case key_w:
    case key_up:
      key = KEY_UPARROW;
      break;
    case key_s:
    case key_down:
      key = KEY_DOWNARROW;
      break;
    case key_control:
    case key_x:
      key = KEY_FIRE;
      break;
    case key_space:
      key = KEY_USE;
      break;
    case key_leftshift:
    case key_rightshift:
      key = KEY_RSHIFT;
      break;
    case key_alt:
      key = KEY_RALT;
      break;
    default:
      break;
  }

  return key;
}



static void addKeyToQueue(int code, bool pressed) {
  unsigned char key = convertToDoomKey(code);
  unsigned short keyData = (pressed << 8) | key;

  // printf("%04x\n", keyData);

  s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
  s_KeyQueueWriteIndex++;
  s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

class doomview : public ui::view {
 public:
  virtual void paint_event(void) override {
    constexpr size_t sz =
        DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t);
    memcpy(window()->bmp().pixels(), DG_ScreenBuffer, sz);
    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override { repaint(); }

  virtual void on_keydown(ui::keydown_event& ev) override {
    addKeyToQueue(ev.code, true);
  }


  virtual void on_keyup(ui::keyup_event& ev) override {
    addKeyToQueue(ev.code, false);
  }
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

  main_widget = &main_window->set_view<doomview>();
}



extern "C" void DG_DrawFrame() {
  main_widget->repaint();
  DG_PumpEventLoop();
}



extern "C" void DG_SleepMs(uint32_t ms) { usleep(ms * 1000); }


extern "C" uint32_t DG_GetTicksMs() {
  return syscall(SYS_gettime_microsecond) / 1000;
}


extern "C" int DG_GetKey(int* pressed, unsigned char* doomKey) {
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
    // key queue is empty

    return 0;
  } else {
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
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
