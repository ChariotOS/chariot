#include <ck/command.h>
#include <ck/dir.h>
#include <ck/eventloop.h>
#include <gfx/font.h>
#include <unistd.h>
#include <ck/time.h>

#include <lumen/ipc.h>
#include <lumen-server/WindowManager.h>
#include "chariot/ck/ptr.h"
#include "lumen-server/ApplicationConnection.h"


int main(int argc, char **argv) async_main({
  auto wm = ck::make_ref<server::WindowManager>();

  // tell the global window manager to listen
  wm->listen("/usr/servers/lumen");

  // spawn some fibers to handle mouse and keyboard input
  ck::spawn([=]() mutable {
    ck::file mouse;
    if (mouse.open("/dev/mouse", "r+")) {
      printf("opened mouse\n");

      while (true) {
        struct mouse_packet pkt;
        mouse.async_read(&pkt, sizeof(pkt)).await();
        if (errno == EAGAIN) break;
        wm->feed_mouse_packet(pkt);
      }
    }
  });

  ck::spawn([=]() mutable {
    printf("opening keyboard\n");
    ck::file keyboard;
    if (keyboard.open("/dev/keyboard", "r+")) {
      printf("opened keyboard\n");

      while (true) {
        keyboard_packet_t pkt;
        keyboard.async_read(&pkt, sizeof(pkt)).await();
        if (errno == EAGAIN) break;
        wm->feed_keyboard_packet(pkt);
      }
    }
  });
})