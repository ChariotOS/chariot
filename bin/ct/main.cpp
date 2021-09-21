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


#if 0


void dump_symbols(int fd) {
  Elf64_Ehdr ehdr;
  lseek(fd, 0, SEEK_SET);
  read(fd, &ehdr, sizeof(ehdr));
  if (ehdr.e_shstrndx == SHN_UNDEF) return;

  for (int i = 0; i < 100; i++) {
    ck::time::logger l("thing");
    Elf64_Phdr* phdr = new Elf64_Phdr[ehdr.e_phnum];
    lseek(fd, ehdr.e_phoff, SEEK_SET);

    size_t hdrs_size;
    hdrs_size = ehdr.e_phnum * ehdr.e_phentsize;
    read(fd, phdr, hdrs_size);

    printf("phdr_size: %llu\n", hdrs_size);

    for (int i = 0; i < ehdr.e_phnum; i++) {
      auto& sec = phdr[i];
      if (sec.p_type == PT_TLS) {
        printf("Found a TLS template\n");
        // printf("  file:%d, mem:%d!\n", sec.p_filesz, sec.p_memsz);
        // printf("  off: %p, vaddr: %p\n", sec.p_offset, sec.p_vaddr);
        // printf("  ali: %p, paddr: %p\n", sec.p_align, sec.p_paddr);

        auto start = sec.p_vaddr;
        int prot = 0;
        if (sec.p_flags & PF_X) prot |= PROT_EXEC;
        if (sec.p_flags & PF_W) prot |= PROT_WRITE;
        if (sec.p_flags & PF_R) prot |= PROT_READ;

        lseek(fd, start, SEEK_SET);
        void* buf = malloc(sec.p_memsz);
        read(fd, buf, sec.p_memsz);
        ck::hexdump(buf, sec.p_memsz);

        free(buf);
      }
    }

    delete[] phdr;
  }

  return;

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


#endif



template <typename T>
async(ck::vec<T>) wait_all(ck::vec<ck::future<T>>& futs) {
  ck::vec<T> res;
  for (auto& fut : futs) {
    res.push(fut.await());
  }
  return res;
}



int main(int argc, char** argv) async_main({
  printf("Connecting...\n");
  auto conn = ck::ipc::connect<lumen::Connection>("/usr/servers/lumen");
  printf("Connected.\n");
  auto [wid] = conn->create_window().unwrap();
  printf("Window Created with wid %d\n", wid);
  conn->set_window_name(wid, "my window name");
})
