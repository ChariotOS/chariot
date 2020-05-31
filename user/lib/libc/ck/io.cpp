#define _CHARIOT_SRC
#include <stdio.h>
#include <ck/io.h>
#include <ck/ptr.h>



ck::buffer::buffer(size_t size) {
  m_buf = calloc(size, 1);
  m_size = size;
}

ck::buffer::~buffer(void) {
  if (m_buf) free(m_buf);
}


ck::buffer ck::stream::read(size_t sz) {
  ck::buffer buf(sz);

  auto n = read(buf.get(), sz);

  if (n < 0) {
    buf.resize(0);
  } else {
    buf.resize(n);
  }
  return buf;
}


ssize_t ck::file::read(void *buf, size_t sz) {
  if (fp == NULL) return -1;
  return fread(buf, 1, sz, fp);
}

ssize_t ck::file::write(const void *buf, size_t sz) {
  if (fp == NULL) return -1;
  return fwrite(buf, 1, sz, fp);
}

ck::file::file(FILE *fp) : fp(fp) {}

ck::file::file(ck::string path, ck::string opts) {
  fp = fopen(path.get(), opts.get());
  printf("fp=%p\n", fp);
}

ck::file::~file(void) {
  if (fp != NULL) fclose(fp);
}


void ck::hexdump(void *buf, size_t sz) {
	debug_hexdump(buf, sz);
}


void ck::hexdump(const ck::buffer &buf) {
	debug_hexdump(buf.get(), buf.size());
}
