#include <ck/dir.h>
#include <errno.h>


ck::directory::directory(void) {
}

bool ck::directory::open(const char *path) {
  // close an already open directory
  if (m_dir != nullptr) {
    closedir(m_dir);
    m_dir = nullptr;
  }
  current_path = path;
  m_dir = opendir(path);
  if (m_dir == nullptr) {
    return false;
  }
  return true;
}

ck::vec<ck::string> ck::directory::all_files(void) {
  ck::vec<ck::string> files;
  pthread_mutex_lock(&m_lock);

  struct dirent *ent;
  while ((ent = readdir(m_dir)) != NULL) {
    ck::string name = ent->d_name;
    if (name == "." || name == "..") continue;
    files.push(move(name));
  }
  rewinddir(m_dir);

  pthread_mutex_unlock(&m_lock);
  return files;
}
