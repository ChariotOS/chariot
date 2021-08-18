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

class ct_window : public ui::window {
 public:
  ct_window(void) : ui::window("current test", 400, 400) {}

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



#include <ck/thread.h>
#include <ck/ipc.h>


static void* ck_fiber_setsp_unoptimisable;

#define set_stack_pointer(x) \
  ck_fiber_setsp_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));



extern "C" void _call_with_new_stack(void* sp, void* func, void* arg);



class fiber : public ck::weakable<fiber> {
  CK_NONCOPYABLE(fiber);
  CK_MAKE_NONMOVABLE(fiber);

 public:
  using Fn = ck::func<void(fiber*)>;


  fiber(Fn fn) : func(move(fn)) {
    stk = (char*)malloc(STACK_SIZE);
    stack_base = stk;
    stack_base += STACK_SIZE;
  }
  ~fiber(void) { free(stk); }

  void yield(void) {
    if (setjmp(ctx) == 0) {
      longjmp(m_return_ctx, 1);
    }
  }

  bool is_done(void) const { return m_done; }
  void resume(void) {
    if (is_done()) {
      fprintf(stderr, "Error: Resuming a complete fiberutine\n");
      return;
    }

    if (setjmp(m_return_ctx) == 0) {
      if (!m_initialized) {
        m_initialized = true;
        // calculate the new stack pointer
        size_t sp = (size_t)(stk);
        sp += STACK_SIZE;
        sp &= ~15;

        set_stack_pointer(sp - 8);
        // func(this);

        _call_with_new_stack((void*)((char*)sp - 8), (void*)fiber_new_stack_callback, (void*)this);
        return;
      }
      longjmp(ctx, 1);
      return;
    }

    return;
  }


 private:
  jmp_buf ctx;
  jmp_buf m_return_ctx;
  bool m_done = false;
  Fn func;


  // static ck::weak_ref<fiber> spawn(Fn) {}

  char* stk = nullptr;
  char* stack_base = nullptr;
  bool m_initialized = false;

  static constexpr long STACK_SIZE = 4096 * 8;


  static void fiber_new_stack_callback(void* _c) {
    fiber* c = (fiber*)_c;
    c->func(c);
    c->m_done = true;
    // switch back to the calling context
    longjmp(c->m_return_ctx, 1);
  }
};


#undef set_stack_pointer

#include <chariot/elf/exec_elf.h>

void dump_symbols(int fd) {
  Elf64_Ehdr ehdr;
  lseek(fd, 0, SEEK_SET);
  read(fd, &ehdr, sizeof(ehdr));

  if (ehdr.e_shstrndx == SHN_UNDEF) return;

  Elf64_Shdr* sec_hdrs = new Elf64_Shdr[ehdr.e_shnum];

  // seek to and read the headers
  lseek(fd, ehdr.e_shoff, SEEK_SET);
  auto sec_expected = ehdr.e_shnum * sizeof(*sec_hdrs);
  auto sec_read = read(fd, sec_hdrs, sec_expected);

  if (sec_read != sec_expected) {
    delete[] sec_hdrs;
    printf("sec_read != sec_expected\n");
    return;
  }

  Elf64_Sym* symtab = NULL;
  int symcount = 0;

  char* strtab = NULL;
  size_t strtab_size = 0;

  for (int i = 0; i < ehdr.e_shnum; i++) {
    auto& sh = sec_hdrs[i];
    if (sh.sh_type == SHT_SYMTAB) {
      symtab = (Elf64_Sym*)malloc(sh.sh_size);

      symcount = sh.sh_size / sizeof(*symtab);

      lseek(fd, sh.sh_offset, SEEK_SET);
      read(fd, symtab, sh.sh_size);
      continue;
    }

    if (sh.sh_type == SHT_STRTAB && strtab == NULL) {
      strtab = (char*)malloc(sh.sh_size);
      lseek(fd, sh.sh_offset, SEEK_SET);
      read(fd, strtab, sh.sh_size);
      strtab_size = sh.sh_size;
    }
  }



  delete[] sec_hdrs;


  int err = 0;
  if (symtab != NULL && symtab != NULL) {
    for (int i = 0; i < symcount; i++) {
      auto& sym = symtab[i];
      if (sym.st_name > strtab_size - 2) {
        err = -EINVAL;
        break;
      }

      const char* tname = "unknown";
      unsigned type = ELF32_ST_TYPE(sym.st_info);
      switch (type) {
        case STT_NOTYPE:
          tname = "NOTYPE";
          break;
        case STT_OBJECT:
          tname = "OBJECT";
          break;
        case STT_FUNC:
          tname = "FUNC";
          break;

        default:
          continue;
          break;
      }

      const char* name = strtab + sym.st_name;
      printf("%4d: 0x%08llx %2d %8s %08x %s\n", i, sym.st_value, type, tname, sym.st_size, name);
    }

    free(symtab);
  }

  if (symtab != NULL) free(symtab);
  if (strtab != NULL) free(strtab);

  return;
}


#include "test.h"



inline auto tsc(void) {
  uint32_t lo, hi;
  /* TODO; */
#ifdef CONFIG_X86
  asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
#endif
  return lo | ((uint64_t)(hi) << 32);
}

class client_connection : public test::client_connection_stub {
 public:
  client_connection(ck::ref<ck::ipcsocket> s) : test::client_connection_stub(s) {}


  ck::option<test::server_compute_response> on_compute(uint32_t val) override {
    // printf("compute %d\n", val);
    return test::server_compute_response({val * 2});
  }
  virtual ~client_connection() {}
};

class server_connection : public test::server_connection_stub {
 public:
  server_connection(ck::ref<ck::ipcsocket> s) : test::server_connection_stub(s) {}

  virtual ~server_connection() {}


  // handle these in your subclass
  ck::option<test::client_map_response> on_map(ck::vec<uint32_t> vec) override {
    // printf("map over %zu values\n", vec.size());
    for (auto& val : vec) {
      // printf("asking to compute on %d\n", val);
      auto r = compute(val);
      if (!r.has_value()) {
        return None;
      }
      val = r.unwrap().result;
    }
    return test::client_map_response{vec};
  }
};


void run_client(ck::string path) {
  ck::eventloop ev;

  ck::ref<ck::ipcsocket> sock = ck::make_ref<ck::ipcsocket>();
  sock->connect(path);
  if (!sock->connected()) panic("ouchie");

  printf("connected!\n");


  client_connection c(sock);
  ck::vec<uint32_t> vec;
  for (int v = 0; v < 100; v++) {
    vec.push(v);
  }
  for (int trial = 0; true; trial++) {
    printf("trial %d, size: %d\n", trial, vec.size());
    test::client_map_response r;
    {
      ck::time::logger l("ipc");
      auto o = c.map(vec);

      if (!o.has_value()) {
        assert(c.closed());
        printf("server dead!\n");
        exit(0);
        break;
      } else {
        r = o.take();
      }
    }

    for (int i = 0; i < vec.size(); i++) {
      assert(vec[i] * 2 == r.vec[i]);
    }
  }

  ev.start();
}


void run_server() {
  ck::eventloop ev;

  ck::ipcsocket server;
  ck::vec<ck::ref<server_connection>> stubs;
  int client_pid = 0;

  ck::string path = ck::string::format("/tmp/ct.%d.sock", rand());

  system(ck::string::format("touch %s", path.get()).get());

  server.listen(path, [&] {
    printf("got a connection!\n");
    auto sock = server.accept();

    stubs.push(ck::make_ref<server_connection>(sock));
  });

  auto t = ck::timer::make_timeout(500, [&server] {
    // die!
    exit(0);
  });

  client_pid = fork();
  if (client_pid == 0) {
    run_client(path);
  }

  printf("client pid = %d\n", client_pid);

  ev.start();
}

int main(int argc, char** argv) {
  int server_pid = fork();

  if (server_pid == 0) {
    run_server();
  }
  waitpid(server_pid, NULL, 0);
  return 0;




#if 0
  auto start = malloc_allocated();
  {
    volatile int i = 0;
    auto f = ck::make_ref<fiber>([&](auto* f) {
      while (1) {
        i++;
        f->yield();
      }
    });

    printf("memory allocated: %llu\n", malloc_allocated() - start);

    for (int i = 0; i < 100; i++) {
      f->resume();
      printf("< %d\n", i);
    }
  }

  printf("memory leaked: %llu\n", malloc_allocated() - start);

  return 0;

#endif


  dump_symbols(MAGICFD_EXEC);

  return 0;

  // auto size = 1024 * 48;
  // for (int i = 0; i < 100; i++) {
  //   auto start = ck::time::us();
  //   void* p = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

  //   auto end = ck::time::us();
  //   printf("%llu us\n", end - start);
  //   munmap(p, size);
  // }
  // return 0;




#if 0
  ck::vec<ck::unique_ptr<ck::thread>> threads;
  for (int i = 0; i < 1000; i++) {
    threads.push(ck::make_unique<ck::thread>([i] {
      printf("in thread %d\n", i);
      return;
    }));
  }

  printf("joining...\n");


  for (auto& thread : threads) {
    thread->join();
  }

  printf("joined.\n");

  return 0;
#endif

  ui::application app;
  ct_window* win = new ct_window;

  app.start();
  return 0;
}
