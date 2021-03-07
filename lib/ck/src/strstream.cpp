#include <ck/strstream.h>
#include <errno.h>

ck::strstream::~strstream() {
}


ssize_t ck::strstream::write(const void *vbuf, size_t sz) {
  auto *buf = (const char *)vbuf;

  // write every char to the buffer
  for (size_t i = 0; i < sz; i++) {
    char c = buf[i];
    // if we're at the end, append to the string
    if (m_index == size()) {
      m_str.push(c);
      m_index++;
    } else {
      m_str[m_index++] = c;
    }
  }

  return sz;
}



ssize_t ck::strstream::read(void *vbuf, size_t size) {
  char *buf = (char *)vbuf;
  size_t nread = 0;

  for (; nread <= size && m_index < m_str.size(); m_index++) {
    buf[nread++] = m_str[m_index];
  }

  return nread;
}


ssize_t ck::strstream::size(void) {
  return m_str.size();
}

ssize_t ck::strstream::tell(void) {
  return m_index;
}

int ck::strstream::seek(long offset, int whence) {
  off_t new_off;

  switch (whence) {
    case SEEK_SET:
      new_off = offset;
      break;
    case SEEK_CUR:
      new_off = m_index + offset;
      break;
    case SEEK_END:
      new_off = size();
      break;
    default:
      new_off = whence + offset;
      break;
  }
  m_index = new_off;
  return m_index;
}


const ck::string &ck::strstream::get(void) {
  return m_str;
}
