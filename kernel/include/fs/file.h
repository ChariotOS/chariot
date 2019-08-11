#ifndef __FILE_H__
#define __FILE_H__

#include <string.h>
#include <vec.h>

namespace fs {


  class file;

  class path {
    public:
      path(string);

      inline const string &operator[](u32 i) {
        return parts[i];
      }

      // how many elements are in the path
      inline u32 len(void) {
        return parts.size();
      }

      inline bool is_root(void) {
        return m_is_root;
      }

      string to_string(void);

    private:


      void parse(string &);
      bool m_is_root = false;

      vec<string> parts;

  };
}

#endif
