#pragma once

#include <ck/ptr.h>
#include <ck/string.h>
#include <stdio.h>
#include <chariot/stat.h>
#include <stdio.h>

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
  class stream {
   public:
    virtual ~stream() {};
    virtual ssize_t write(const void *buf, size_t) = 0;
    virtual ssize_t read(void *buf, size_t) = 0;

		// helper functions implemented by stream
		ck::buffer read(size_t);
  };

  class file : public ck::stream {
    // this defers to stdio :^) the work has been done!
    FILE *fp = NULL;

   public:
    // construct by taking ownership of a file
    file(FILE *fp);
		// construct by opening the file
    file(ck::string path, ck::string opts);
    virtual ~file(void);

    virtual ssize_t write(const void *buf, size_t) override;
    virtual ssize_t read(void *buf, size_t) override;

		// the size of this file
		ssize_t size(void);
		// current location in the file
		ssize_t tell(void);
		int seek(long offset, int whence);
		int stat(struct stat &);

		int writef(const char *format, ...);

		using stream::read;

    inline operator bool(void) const { return fp != NULL; }
  };


	// print a nice and pretty hexdump to the screen
	void hexdump(void *buf, size_t);
	// hexdump a ck::buffer (special case)
	void hexdump(const ck::buffer &);

	// hexdump a struct :^)
	template<typename T>
		void hexdump(const T &val) {
			ck::hexdump((void*)&val, sizeof(T));
		}

}  // namespace ck
