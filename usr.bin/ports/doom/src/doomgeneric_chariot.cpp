#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>


#include <ui/application.h>
#include <ui/view.h>


extern "C" {
#include "doomgeneric.h"
}

static ui::application main_app;
static ui::window *main_window;
static ui::view *main_widget;





class doomview : public ui::view {
 public:

  virtual void paint_event(void) override {
		constexpr size_t sz = DOOMGENERIC_RESX * DOOMGENERIC_RESY * sizeof(uint32_t);
		memcpy(window()->bmp().pixels(), DG_ScreenBuffer, sz);
    invalidate();
  }

  virtual void on_mouse_move(ui::mouse_event& ev) override {
    repaint();
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
	main_window = main_app.new_window("DOOM", DOOMGENERIC_RESX, DOOMGENERIC_RESY);

	main_widget = &main_window->set_view<doomview>();
}



extern "C" void DG_DrawFrame() {
	main_widget->repaint();
	DG_PumpEventLoop();
}



extern "C" void DG_SleepMs(uint32_t ms) {
  usleep(ms * 1000);
}


extern "C" uint32_t DG_GetTicksMs() {
  return syscall(SYS_gettime_microsecond) / 1000;
}


extern "C" int DG_GetKey(int* pressed, unsigned char* key) { return 0; }


extern "C" void DG_SetWindowTitle(const char* title) {
	printf("set window title to '%s'\n", title);
}
