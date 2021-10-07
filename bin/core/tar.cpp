#include <ck/io.h>
#include <stdio.h>


// TAR does some nonsense with octal, idk
unsigned int getsize(const char *in) {
  unsigned int size = 0;
  unsigned int j;
  unsigned int count = 1;

  for (j = 11; j > 0; j--, count *= 8)
    size += ((in[j - 1] - '0') * count);

  return size;
}

struct tar_header {
  char filename[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag[1];


  unsigned int getsize() { return ::getsize(size); }
  inline char *content(void) { return (char *)this + 512; }
};




int main() {
  ck::file file;

  if (!file.open("/root/hello.tar", "r")) {
    ck::err.fmt("Failed to open file.\n");
    return -1;
  }


  auto m = file.mmap();


  ck::vec<struct tar_header *> files;

  unsigned int i;
  char *address = m->as<char>();

  for (i = 0;; i++) {
    struct tar_header *header = (struct tar_header *)address;

    if (header->filename[0] == '\0') break;
    unsigned int size = header->getsize();
    printf("'%s' %d\n", header->filename, size);
    files.push(header);
    address += ((size / 512) + 1) * 512;

    if (size % 512) address += 512;
  }


  for (auto *file : files) {
    ck::hexdump(file->content(), file->getsize());
  }

  return 0;
}
