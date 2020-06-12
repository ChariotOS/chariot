#define _CHARIOT_SRC
#include <stdio.h>
#include <ck/io.h>
#include <ck/ptr.h>
#include <sys/stat.h>



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

ssize_t ck::file::size(void) {
	if (fp == NULL) return -1;

	auto original = tell();
	if (fseek(fp, 0, SEEK_END) == -1) return -1;
	auto sz = ftell(fp);
	// not sure how we handle an error here, we may have broken it
	// with the first call to fseek
	if (fseek(fp, original, SEEK_SET) == -1) return -1;
	return sz;
}

ssize_t ck::file::tell(void) {
	if (fp == NULL) return -1;
	return ftell(fp);
}

int ck::file::seek(long offset, int whence) {

	return fseek(fp, offset, whence);
}

int ck::file::stat(struct stat &s) {
	int fd = ::fileno(fp);

	return fstat(fd, &s);
}

int ck::file::writef(const char *format, ...) {
	va_list va;
	va_start(va, format);
	const int ret = vsnfprintf(fp, format, va);
	va_end(va);
	return ret;

}

ck::file::file(FILE *fp) : fp(fp) {}

ck::file::file(ck::string path, ck::string opts) {
  fp = fopen(path.get(), opts.get());
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
