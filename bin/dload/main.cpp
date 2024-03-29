#define _CHARIOT_SRC
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exec_elf.h"

int try_file(const char *);




#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))
// allocation only, no free, as nearly all things in the dynamic linker is long-lived
static void *ld_malloc(long sz) {
  sz = ALIGN(sz, 16);
  static long heap_remaining = 0;
  static void *heap_top = NULL;

  if (heap_remaining - sz < 0 || heap_top == NULL) {
    // allocate 32kb (8 pages) and throw away the old heap_top
    heap_remaining = 8 * 4096;
    heap_top = mmap(NULL, heap_remaining, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
  }

  void *ptr = heap_top;
  heap_top = (void *)((char *)heap_top + sz);
  heap_remaining -= sz;

  return ptr;
}




int main(int argc, char **argv) {
  int *x = (int *)ld_malloc(sizeof(int));

  *x = 40;

  printf("x: %d\n", *x);
  try_file("/lib/libshared.so");

  // return 0;
  for (int i = 1; i < argc; i++) {
    try_file(argv[i]);
  }
  return 0;
}




int try_file(const char *path) {
  struct stat st;
  if (lstat(path, &st) < 0) return -1;

  int fd = open(path, O_RDONLY);
  if (fd < 0) return -1;

  void *buf = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (buf == MAP_FAILED) {
    fprintf(stderr, "Failed to map file.\n");
    exit(EXIT_FAILURE);
  }

  if (memcmp(buf, ELFMAG, 4)) exit(EXIT_FAILURE);

  Elf64_Ehdr *e = (Elf64_Ehdr *)buf;
  if (e->e_machine != EM_AMD64) {
    fprintf(stderr, "Invalid machine type (must be amd64 for now)\n");
    exit(EXIT_FAILURE);
  }


#define P(name) printf("%12s: 0x%-8llx %llu\n", #name, e->name, e->name)
  P(e_type);
  P(e_machine);
  P(e_version);
  P(e_phoff);
  P(e_entry);
  P(e_flags);
  P(e_ehsize);
  P(e_phentsize);
  P(e_phnum);
  P(e_shentsize);
  P(e_shnum);
  P(e_shstrndx);
#undef P


  Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)buf + e->e_phoff);

  printf("Entry: %llx\n", e->e_entry);

  for (int i = 0; i < e->e_phnum; i++) {
    Elf64_Phdr *p = phdrs + i;

    printf("type: %d\n", p->p_type);


    if (p->p_type == PT_LOAD) {
      printf("PT_LOAD\n");
    }


    if (p->p_type == PT_INTERP) {
      printf("PT_INTERP\n");
    }

    if (p->p_type == PT_DYNAMIC) {
      printf("PT_DYNAMIC\n");
      Elf64_Dyn *d = (Elf64_Dyn *)((char *)buf + p->p_offset);

      for (int i = 0; i < p->p_filesz / sizeof(*d); i++) {
        // break on DT_NULL
        if (d[i].d_tag == DT_NULL) break;
        // printf("tag=%-10llx val=%-10llx\n", d[i].d_tag, d[i].d_un.d_val);

        switch (d[i].d_tag) {
          case DT_SYMTAB:
            printf("symtab %llx\n", d[i].d_un.d_val);
            break;

          case DT_STRTAB:
            printf("strtab %llx\n", d[i].d_un.d_val);
            break;


          default:
            break;
        }
      }
    }

    // debug_hexdump((char *)buf + p->p_offset, p->p_filesz);
  }


  munmap(buf, st.st_size);
  close(fd);
  return 0;
}
