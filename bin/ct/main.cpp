/*

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

#include <scm/scm.h>
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


async(int) make_fiber(int num, int size, int div) {
  return ck::spawn<int>([num, size, div]() -> int {
    if (size == 1) {
      return num;
    }

    ck::vec<ck::future<int>> futs;
    futs.resize(div);

    int sum = 0;
    for (int i = 0; i < div; i++) {
      auto sub_num = num + (i * (size / div));
      futs.push(make_fiber(sub_num, size / div, div));
    }
    return sum;
    for (auto& fut : futs)
      sum += fut.await();
    return sum;
  });
}





void* thread_throughput_test(void*) {
  while (1) {
  }
  char buf[256];
  int tid = gettid();
  auto last = ms();
  long count = 0;
  float measure_fract = 0.1;
  unsigned long interval = 1000 * measure_fract;
  while (1) {
    ck::thread t([] {
    });
    t.join();
    count++;

    auto now = ms();

    if (now - last > interval) {
      unsigned long long avail, total;
      sysbind_getraminfo(&avail, &total);
      // printf("created %ld threads/second\n", count);
      snprintf(buf, 512, "%d created %ld threads/second. %llu\n", tid, (long)(count * (1 / measure_fract)), avail);
      write(1, buf, strlen(buf));
      last = now;
      count = 0;
    }
  }
  return nullptr;
}


int main(int argc, char** argv) async_main({


        return 0;
#if 0
  // thread_throughput_test(nullptr);
  auto nproc = sysbind_get_nproc();
  printf("nproc: %d\n", nproc);



  while (1) {
    int pid = fork();

    if (pid == 0) {
      exit(0);
      pthread_t threads[nproc];
      for (int i = 0; i < nproc; i++)
        pthread_create(&threads[i], NULL, thread_throughput_test, NULL);

      for (int i = 0; i < nproc; i++)
        pthread_join(threads[i], NULL);
    } else {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
                        printf("killed %d\n", pid);
    }
  }


#if 0
  auto start = sysbind_gettime_microsecond();
  int result = make_fiber(0, 1000000, 10).await();
  auto end = sysbind_gettime_microsecond();
  printf("Skynet Result: %d in %fms\n", result, (end - start) / 1000.0f);

  return;

#endif
  printf("Connecting...\n");
  auto conn = ck::ipc::connect<lumen::Connection>("/usr/servers/lumen");
  printf("Connected.\n");
  auto [wid] = conn->create_window().unwrap();
  printf("Window Created with wid %d\n", wid);
  conn->set_window_name(wid, "my window name");
#endif
})
*/


#include <wasm.h>
#include "../../lib/wasm/src/m3_config.h"
#include "../../lib/wasm/src/m3_api_libc.h"
#include "../../lib/wasm/src/m3_api_wasi.h"
#include <stdio.h>
#include <sys/sysbind.h>
#include <ck/io.h>

#include "./coremark.wasm.h"

unsigned long ms() { return sysbind_gettime_microsecond() / 1000; }

#define FATAL(msg, ...)                        \
  {                                            \
    printf("Fatal: " msg "\n", ##__VA_ARGS__); \
    return;                                    \
  }

// clang-format off
unsigned char fib32_wasm[] = {
	0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x06, 0x01, 0x60, 0x01,
	0x7f, 0x01, 0x7f, 0x03, 0x02, 0x01, 0x00, 0x07, 0x07, 0x01, 0x03, 0x66, 0x69,
	0x62, 0x00, 0x00, 0x0a, 0x1f, 0x01, 0x1d, 0x00, 0x20, 0x00, 0x41, 0x02, 0x49,
	0x04, 0x40, 0x20, 0x00, 0x0f, 0x0b, 0x20, 0x00, 0x41, 0x02, 0x6b, 0x10, 0x00,
	0x20, 0x00, 0x41, 0x01, 0x6b, 0x10, 0x00, 0x6a, 0x0f, 0x0b
};
unsigned int fib32_wasm_len = 62;
// clang-format on


void run_fib() {
  M3Result result = m3Err_none;

  uint8_t* wasm = (uint8_t*)fib32_wasm;
  uint32_t fsize = fib32_wasm_len;

  printf("Loading WebAssembly...\n");
  IM3Environment env = m3_NewEnvironment();
  if (!env) FATAL("m3_NewEnvironment failed");

  IM3Runtime runtime = m3_NewRuntime(env, 1024, NULL);
  if (!runtime) FATAL("m3_NewRuntime failed");

  IM3Module module;
  result = m3_ParseModule(env, &module, wasm, fsize);
  if (result) FATAL("m3_ParseModule: %s", result);

  result = m3_LoadModule(runtime, module);
  if (result) FATAL("m3_LoadModule: %s", result);

  IM3Function f;
  result = m3_FindFunction(&f, runtime, "fib");
  if (result) FATAL("m3_FindFunction: %s", result);

  printf("Running...\n");

  result = m3_CallV(f, 24);
  if (result) FATAL("m3_Call: %s", result);

  uint32_t value = 0;
  result = m3_GetResultsV(f, &value);
  if (result) FATAL("m3_GetResults: %s", result);

  printf("Result: %d\n", value);
}



void run_rust() {
  M3Result result = m3Err_none;

  ck::file file;
  if (!file.open("/wasm-test.wasm", "r")) {
    FATAL("Failed to open file\n");
  }

  auto mapping = file.mmap();

  uint8_t* wasm = (uint8_t*)mapping->data();
  uint32_t fsize = mapping->size();
  // printf("wasm: %p, sz: %zu\n", wasm, fsize);

  IM3Environment env = m3_NewEnvironment();
  if (!env) FATAL("m3_NewEnvironment failed");

  IM3Runtime runtime = m3_NewRuntime(env, 8192, NULL);
  if (!runtime) FATAL("m3_NewRuntime failed");

  IM3Module module;
  result = m3_ParseModule(env, &module, wasm, fsize);
  if (result) FATAL("m3_ParseModule: %s", result);

  result = m3_LoadModule(runtime, module);
  if (result) FATAL("m3_LoadModule: %s", result);

  result = m3_LinkWASI(module);
  if (result) FATAL("m3_LinkWASI: %s", result);

  result = m3_LinkLibC(module);
  if (result) FATAL("m3_LinkLibC: %s", result);

  IM3Function f;
  result = m3_FindFunction(&f, runtime, "_start");
  if (result) FATAL("m3_FindFunction: %s", result);


  printf("Running Rust through wasm...\n");
  result = m3_CallV(f);
  if (result) FATAL("m3_Call: %s", result);
}


void run_coremark() {
  M3Result result = m3Err_none;

  ck::file file;
  if (!file.open("/wasm-test.wasm", "r")) {
    FATAL("Failed to open file\n");
  }

  auto mapping = file.mmap();

  uint8_t* wasm = (uint8_t*)coremark_minimal_wasm;  // mapping->data();
  uint32_t fsize = coremark_minimal_wasm_len;       // mapping->size();
  printf("wasm: %p, sz: %zu\n", wasm, fsize);

  printf("Loading WebAssembly...\n");

  IM3Environment env = m3_NewEnvironment();
  if (!env) FATAL("m3_NewEnvironment failed");

  IM3Runtime runtime = m3_NewRuntime(env, 8192, NULL);
  if (!runtime) FATAL("m3_NewRuntime failed");

  IM3Module module;
  printf("Parsing Module...\n");
  result = m3_ParseModule(env, &module, wasm, fsize);
  if (result) FATAL("m3_ParseModule: %s", result);

  printf("Loading Module...\n");
  result = m3_LoadModule(runtime, module);
  if (result) FATAL("m3_LoadModule: %s", result);

  printf("Linking Libc...\n");
  result = m3_LinkLibC(module);
  if (result) FATAL("m3_LinkLibC: %s", result);

  printf("Finding Function...\n");
  IM3Function f;
  result = m3_FindFunction(&f, runtime, "run");
  if (result) FATAL("m3_FindFunction: %s", result);

  printf("Running CoreMark 1.0...\n");

  result = m3_CallV(f);
  if (result) FATAL("m3_Call: %s", result);

  float value = 0;
  result = m3_GetResultsV(f, &value);
  if (result) FATAL("m3_GetResults: %s", result);

  printf("Result: %0.3f\n", value);
}


int main(int argc, char** argv) {
  auto start = ms();
  // run_fib();
  // run_coremark();
	run_rust();
  auto end = ms();

  printf("Elapsed: %ld ms\n", end - start);
  return 0;
}
