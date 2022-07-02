#include "ck/event.h"
#define CT_TEST
#include <ui/application.h>
#include <gfx/geom.h>
#include <ui/boxlayout.h>
#include "sys/sysbind.h"
#include "chariot/ck/ptr.h"
#include "ck/eventloop.h"
#include "lumen.h"
#include "stdlib.h"
#include "ui/event.h"
#include "ui/textalign.h"
#include <ck/timer.h>
#include <ui/frame.h>
#include <ck/pair.h>
#include <ui/label.h>
#include <ck/json.h>
#include <ck/time.h>
#include <sys/sysbind.h>
#include <alloca.h>
#include <errno.h>
#include <sys/syscall.h>
#include <chariot/fs/magicfd.h>
#include <sys/wait.h>
#include <ck/thread.h>
#include <ck/ipc.h>
#include <chariot/elf/exec_elf.h>
#include <ck/resource.h>
#include <fcntl.h>
#include <ck/future.h>
#include <lumen/ipc.h>
#include "test.h"

#include <vterm.h>

unsigned long ms() { return sysbind_gettime_microsecond() / 1000; }

#if 0

class ct_window : public ui::window {
 public: ct_window(void) : ui::window("current test", 400, 400) {}

  virtual ck::ref<ui::view> build(void) {
    // clang-format off

    ui::style st = {
      .background = 0xE0E0E0,
      .margins = 10,
      .padding = 10,
    };

    return ui::make<ui::vbox>({
      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Top Left", ui::TextAlign::TopLeft),
          ui::make<ui::label>("Top Center", ui::TextAlign::TopCenter),
          ui::make<ui::label>("Top Right", ui::TextAlign::TopRight),
        }),

      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Left", ui::TextAlign::CenterLeft),
          ui::make<ui::label>("Center", ui::TextAlign::Center),
          ui::make<ui::label>("Right", ui::TextAlign::CenterRight),
        }),


      ui::make<ui::hbox>(st, {
          ui::make<ui::label>("Bottom Left", ui::TextAlign::BottomLeft),
          ui::make<ui::label>("Bottom Center", ui::TextAlign::BottomCenter),
          ui::make<ui::label>("Bottom Right", ui::TextAlign::BottomRight),
        }),

    });

    // clang-format on
  }
};


#endif



template <typename T>
async(ck::vec<T>) wait_all(ck::vec<ck::future<T>> &futs) {
  ck::vec<T> res;
  for (auto &fut : futs) {
    res.push(fut.await());
  }
  return res;
}

uint64_t nalive = 0;
async(int) make_fiber(int num, int size, int div) {
  nalive++;
  if (size == 1) {
    nalive--;
    return num;
  }


  return ck::spawn<int>([num, size, div]() -> int {
    ck::vec<ck::future<int>> futs;
    futs.resize(div);

    int sum = 0;
    for (int i = 0; i < div; i++) {
      auto sub_num = num + (i * (size / div));
      futs.push(make_fiber(sub_num, size / div, div));
    }
    // return sum;
    for (auto &fut : futs) {
      sum += fut.await();
      if (nalive % 1000 == 0) {
        printf("%lld\n", nalive);
      }
    }
    nalive--;
    return sum;
  });
}

int nproc = 0;


#include <chariot/futex.h>
#include <atomic.h>

int main(int argc, char **argv) {
  int rows = 10;
  int cols = 20;
  VTerm *vt = vterm_new(rows, cols);
  vterm_set_utf8(vt, 1);
  VTermScreen *screen = vterm_obtain_screen(vt);
	/*
  VTermScreenCallbacks cbs{0};
  cbs.damage = [](VTermRect rect, void *user) -> int {
    printf("damage\n");
    return 0;
  };

  cbs.moverect = [](VTermRect dest, VTermRect src, void *user) -> int {
    printf("moverect\n");
    return 0;
  };

  cbs.movecursor = [](VTermPos pos, VTermPos oldpos, int visible, void *user) -> int {
    printf("movecursor\n");
    return 0;
  };

  cbs.settermprop = [](VTermProp prop, VTermValue *val, void *user) -> int {
    printf("settermprop\n");
    return 0;
  };

  cbs.bell = [](void *user) -> int {
    printf("bell\n");
    return 0;
  };

  cbs.resize = [](int rows, int cols, void *user) -> int {
    printf("resize\n");
    return 0;
  };

  cbs.sb_pushline = [](int cols, const VTermScreenCell *cells, void *user) -> int {
    printf("pushline\n");
    return 0;
  };

  cbs.sb_popline = [](int cols, VTermScreenCell *cells, void *user) -> int {
    printf("popline\n");
    return 0;
  };
  vterm_screen_set_callbacks(screen, &cbs, NULL);
	*/


  char buf[] = "Hello friend\n";
  vterm_input_write(vt, buf, strlen(buf));

  for (int row = 0; row < rows; row++) {
    for (int col = 0; col < cols; col++) {
      VTermPos pos;
      pos.col = col;
      pos.row = row;
      VTermScreenCell cell{0};
      // vterm_screen_get_cell(screen, pos, &cell);
      if (cell.width > 0) {
        for (int i = 0; i < cell.width; i++) {
          printf("%02x ", cell.chars[i]);
        }
      } else {
        printf(" ");
      }
    }
    printf("\n");
  }



  vterm_free(vt);
  return 0;
}


/*

int main(int argc, char** argv) async_main({

  uint64_t start = sys::gettime_microsecond();
  int result = make_fiber(0, 100000, 10).await();
  uint64_t end = sys::gettime_microsecond();
  printf("Skynet Result: %d in %fms\n", result, (end - start) / 1000.0f);
  return;

#if 0

  return;

  printf("Connecting...\n");
  auto conn = ck::ipc::connect<lumen::Connection>("/usr/servers/lumen");
  printf("Connected.\n");
  auto [wid] = conn->create_window().unwrap();
  printf("Window Created with wid %d\n", wid);
  conn->set_window_name(wid, "my window name");
#endif
})
*/
