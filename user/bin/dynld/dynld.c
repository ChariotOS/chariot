#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exec_elf.h"

int try_file(const char *);

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    try_file(argv[i]);
  }
  return 0;
}


void hexdump(void *vbuf, long len) {
  unsigned char *buf = vbuf;

  int w = 16;
  for (int i = 0; i < len; i += w) {

    unsigned char *line = buf + i;
    printf("%p: ", (void*)(long)i);
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
        printf("   ");
      } else {
        printf("%02x ", line[c]);
      }
    }
    printf(" |");
    for (int c = 0; c < w; c++) {
      if (i + c >= len) {
      } else {
        printf("%c", (line[c] < 0x20) || (line[c] > 0x7e) ? '.' : line[c]);
      }
    }
    printf("|\n");
  }
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

  Elf64_Ehdr *e = buf;
  if (e->e_machine != EM_AMD64) {
    fprintf(stderr, "Invalid machine type (must be amd64 for now)\n");
    exit(EXIT_FAILURE);
  }

  Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)buf + e->e_phoff);

  printf("Entry: %llx\n", e->e_entry);

  for (int i = 0; i < e->e_phnum; i++) {
    Elf64_Phdr *p = phdrs + i;


		if (p->p_type == PT_LOAD) {
			printf("PT_LOAD\n");
		}


    if (p->p_type == PT_INTERP) {
      printf("PT_INTERP\n");
      hexdump((char*)buf + p->p_offset, p->p_filesz);
    }

    if (p->p_type == PT_DYNAMIC) {
      printf("PT_DYNAMIC\n");
      Elf64_Dyn *d = (Elf64_Dyn*)((char*)buf + p->p_offset);

      for (int i = 0; i < p->p_filesz / sizeof(*d); i++) {
        // break on DT_NULL
        if (d[i].d_tag == DT_NULL) break;
        // printf("tag=%-10llx val=%-10llx\n", d[i].d_tag, d[i].d_un.d_val);

        switch(d[i].d_tag) {

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

  }


  munmap(buf, st.st_size);
  close(fd);
  return 0;
}
