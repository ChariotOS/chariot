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
async(ck::vec<T>) wait_all(ck::vec<ck::future<T>>& futs) {
  ck::vec<T> res;
  for (auto& fut : futs) {
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
    for (auto& fut : futs) {
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



static inline uint32_t cmpxchg32(void* m, uint32_t old, uint32_t newval) {
  uint32_t ret;

  ret = __atomic_compare_exchange_n((uint32_t*)m, &old, newval, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);  // ? newval : old;

  /* asm volatile ("movl %[_old], %%eax\n\t" */
  /*               "cmpxchgl %[_new], %[_m]\n\t" */
  /*               "movl %%eax, %[_out]" */
  /*               : [_out] "=r" (ret) */
  /*               : [_old] "r" (old), */
  /*                 [_new] "r" (new), */
  /*                 [_m]   "m" (m) */
  /*               : "rax", "memory"); */

  return ret;
}

int futex_wait(int* ptr, int value) { return sys::futex(ptr, FUTEX_WAIT, value, 0, 0, 0); }

int futex_wake(int* ptr, int value) { return sys::futex(ptr, FUTEX_WAKE, value, 0, 0, 0); }

class FastMutex {
 public:
  FastMutex() : m_word(0) {}
  void lock() {
    // try to atimically swap 0 -> 1
    if (cmpxchg32(&m_word, 0, 1)) return;  // success
    // wasn't zero -- somebody held the lock
    do {
      // assume lock is still taken, try to make it 2 and wait
      if (m_word == 2 || cmpxchg32(&m_word, 1, 2)) {
        // let's wait, but only if the value is still 2
        futex_wait(&m_word, 2);
      }
      // try (again) assuming the lock is free
    } while (!cmpxchg32(&m_word, 0, 2));
    // we are here only if transition 0 -> 2 succeeded
  }
  void unlock() {
    int res = __atomic_fetch_sub(&m_word, 1, __ATOMIC_SEQ_CST);
    // we own the lock, so it's either 1 or 2
    if (res != 1) {
      // it was 2
      // important: we are still holding the lock!
      m_word = 0;  // not any more
      // wake up one thread (no fairness assumed)
      futex_wake(&m_word, 1);
    }
  }

 private:
  int m_word;
};




volatile int val;
FastMutex mut;

// ck::futex x;
void* work(void* p) {
  long me = (long)p;
  printf("thread %d\n", me);


  for (int i = 0; 1; i++) {
    int x = rand() % 100;
    // mut.lock();
    val = x * x;
    // assert(val == x * x);
    // mut.unlock();
  }


  return NULL;
}

int main(int argc, char** argv) {
  nproc = sysbind_get_nproc();
  __sync_synchronize();
  printf("nproc: %d\n", nproc);
  auto start = sys::gettime_microsecond();

  pthread_t threads[nproc];

  for (long i = 0; i < nproc; i++) {
    pthread_create(&threads[i], NULL, work, (void*)i);
  }

  for (long i = 0; i < nproc; i++) {
    pthread_join(threads[i], NULL);
  }
  auto end = sys::gettime_microsecond();
  printf("%lu us\n", end - start);

  //
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
