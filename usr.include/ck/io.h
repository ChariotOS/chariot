#pragma once

#include <chariot/stat.h>
#include <ck/object.h>
#include <ck/ptr.h>
#include <ck/string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <ck/fsnotifier.h>

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

    inline bool eof(void) { return m_eof; }

   protected:
    inline void set_eof(bool e) { m_eof = e; }


		CK_OBJECT(ck::stream);
  };


  // A file is an "abstract implementation" of
  class file : public ck::stream {
   public:
		// construct without opening any file
		file(void);
    // construct by opening the file
    file(ck::string path, const char *mode);
    // give the file ownership of a file descriptor
    file(int fd);

    virtual ~file(void);

    virtual ssize_t write(const void *buf, size_t) override;
    virtual ssize_t read(void *buf, size_t) override;

    bool open(ck::string path, const char *mode);

		static inline auto unowned(int fd) {
			auto f = ck::file::create(fd);
			f->m_owns = false;
			return f;
		}

    // the size of this file
    ssize_t size(void);
    // current location in the file
    ssize_t tell(void);
    int seek(long offset, int whence);
    int stat(struct stat &);

    int writef(const char *format, ...);

    using stream::read;

    inline operator bool(void) const { return m_fd != -1; }

    inline int fileno(void) { return m_fd; }

    void flush(void);

    // if size == 0, disable buffer.
    // otherwise allocate a buffer.
    void set_buffer(size_t size);
    inline bool buffered(void) { return buf_cap > 0; }

		// callback functions for read/write
    ck::func<void()> on_read;
    ck::func<void()> on_write;

   protected:
		bool m_owns = true;
    int m_fd = -1;
    size_t buf_cap = 0;
    size_t buf_len = 0;
    uint8_t *m_buffer = NULL;

		ck::fsnotifier notifier;

		void init_notifier(void);

		CK_OBJECT(ck::file);
  };




	extern ck::file stdin;
	extern ck::file stdout;
	extern ck::file stderr;


  // print a nice and pretty hexdump to the screen
  void hexdump(void *buf, size_t);
  // hexdump a ck::buffer (special case)
  void hexdump(const ck::buffer &);

  // hexdump a struct :^)
  template <typename T>
  void hexdump(const T &val) {
    ck::hexdump((void *)&val, sizeof(T));
  }

}  // namespace ck
