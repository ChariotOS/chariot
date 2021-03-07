#pragma once

#include <ck/io.h>
#include <ck/string.h>

namespace ck {

  class strstream final : public ck::stream {
   private:
    ck::string m_str;
    off_t m_index = 0;

   public:
    virtual ~strstream();

    virtual ssize_t write(const void *buf, size_t) override;
    virtual ssize_t read(void *buf, size_t) override;
    virtual ssize_t size(void) override;
    virtual ssize_t tell(void) override;
    virtual int seek(long offset, int whence) override;

    // just return the buffer :)
    const ck::string &get(void);
    inline const char *buf(void) {
      return get().get();
    }
  };

}  // namespace ck
