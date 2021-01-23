#pragma once

#include <chariot/stat.h>
#include <ck/fsnotifier.h>
#include <ck/object.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

namespace ck {

  class buffer {
    void *m_buf = NULL;
    size_t m_size = 0;

   public:
    buffer(size_t);
    ~buffer(void);

    template <typename T>
    inline T *get() const {
      return (T *)get();
    }

    inline void *get(void) const { return m_buf; }
    inline size_t size(void) const { return m_size; }

    inline void resize(size_t s) {
      if (s == 0) {
        free(m_buf);
        m_buf = NULL;
        m_size = 0;
      } else {
        m_buf = realloc(m_buf, s);
        m_size = s;
      }
    }
  };

  // an abstract type which can be read from and written to
  class stream : public ck::object {
    bool m_eof = false;

   public:
    virtual ~stream(){};
    inline virtual ssize_t write(const void *buf, size_t) { return -1; }
    inline virtual ssize_t read(void *buf, size_t) { return -1; }
    virtual ssize_t size(void) { return 0; }
    virtual ssize_t tell(void) { return 0; }
    virtual int seek(long offset, int whence) { return -1; }

    inline bool eof(void) { return m_eof; }

    int fmt(const char *format, ...);

    inline int getc(void) {
      if (m_eof) return -1;
      char c = 0;
      this->read(&c, 1);
      if (m_eof) return -1;
      return c;
    }

   protected:
    inline void set_eof(bool e) { m_eof = e; }


    CK_OBJECT(ck::stream);
  };




  // A file is an "abstract implementation" of
  class file : public ck::stream {
   public:
    class mapping {
     public:
      friend class file;

      template <typename T>
      constexpr T *as(off_t loc = 0) {
				if (mem == NULL) return NULL;
        return (T *)((char*)mem + loc);
      }

      inline void *data() { return mem; }


      inline auto size(void) { return len; }
      ~mapping();

     protected:
      mapping(void *mem, size_t len) : mem(mem), len(len) {}
      void *mem;
      size_t len;
    };

    // construct without opening any file
    file(void);
    // construct by opening the file
    file(ck::string path, const char *mode);
    // give the file ownership of a file descriptor
    file(int fd);

    virtual ~file(void);

    virtual ssize_t write(const void *buf, size_t) override;
    virtual ssize_t read(void *buf, size_t) override;
    virtual ssize_t size(void) override;
    virtual ssize_t tell(void) override;
    virtual int seek(long offset, int whence) override;

    bool open(ck::string path, const char *mode);

    static inline auto unowned(int fd) {
      auto f = ck::file::create(fd);
      f->m_owns = false;
      return f;
    }



    inline ck::unique_ptr<ck::file::mapping> mmap(/* Whole file */) {
      return mmap(0, size());
    }
    ck::unique_ptr<ck::file::mapping> mmap(off_t off, size_t len);

    int stat(struct stat &);


    using stream::read;

    inline operator bool(void) const { return m_fd != -1; }

    inline int fileno(void) { return m_fd; }

    void flush(void);

    template <typename T>
    int ioctl(int req, T arg) {
      return ioctl(m_fd, req, arg);
    }

    // if size == 0, disable buffer.
    // otherwise allocate a buffer.
    void set_buffer(size_t size);
    inline bool buffered(void) { return buf_cap > 0; }


    inline auto on_read(ck::func<void()> cb) {
      m_on_read = move(cb);
      update_notifier();
    }


    inline auto on_write(ck::func<void()> cb) {
      m_on_write = move(cb);
      update_notifier();
    }

   protected:
    // callback functions for read/write
    ck::func<void()> m_on_read;
    ck::func<void()> m_on_write;


    bool m_owns = true;
    int m_fd = -1;
    size_t buf_cap = 0;
    size_t buf_len = 0;
    uint8_t *m_buffer = NULL;

    ck::fsnotifier notifier;

    void init_notifier(void);
    void update_notifier(void);

    CK_OBJECT(ck::file);
  };


  extern ck::file in;
  extern ck::file out;
  extern ck::file err;



  // print a nice and pretty hexdump to the screen
  void hexdump(void *buf, size_t, int grouping = 1);

  // hexdump a ck::buffer (special case)
  void hexdump(const ck::buffer &);

  // hexdump a struct :^)
  template <typename T>
  void hexdump(const T &val) {
    ck::hexdump((void *)&val, sizeof(T), sizeof(T));
  }



  template <typename T>
  void hexdump_array(T *val, size_t count) {
    ck::hexdump((void *)val, count * sizeof(T), sizeof(T));
  }




}  // namespace ck
