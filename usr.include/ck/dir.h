#pragma once

#include <dirent.h>
#include <ck/vec.h>
#include <ck/string.h>
#include <pthread.h>

namespace ck {
  class directory {
		ck::string current_path;
		struct __dirstream *m_dir = nullptr;
		pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

   public:
    // no constructor. you must explicitly open it with ::open
    directory();

    bool open(const char *path);
		ck::vec<ck::string> all_files(void);
  };
};  // namespace ck
