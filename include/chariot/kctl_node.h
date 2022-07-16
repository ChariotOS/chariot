#pragma once


#include <kctl.h>
#include <asm.h>
#include <types.h>
#include <ck/string.h>

namespace kctl {
  class Path {
   public:
    Path(void) : m_path(NULL), m_len(0) {}
    Path(off_t *path, size_t len) : m_path(path), m_len(len) {}
    bool is_leaf(void) const { return m_len == 1; }
    kctl::Path next(void) const {
      if (is_leaf()) return {};
      return kctl::Path(m_path + 1, m_len - 1);
    }


    operator bool(void) const { return m_len != 0; }
    off_t operator[](off_t offset) const { return m_path[offset]; }

   private:
    off_t *m_path;
    size_t m_len;
  };

  class Node {
   public:
    virtual bool kctl_read(kctl::Path path, ck::string &out) { return false; }
    virtual bool kctl_writable(void) { return false; }
  };


  class Directory : public kctl::Node {
    bool kctl_read(kctl::Path path, ck::string &out) override {
      //
      return false;
    }
  };

}  // namespace kctl


