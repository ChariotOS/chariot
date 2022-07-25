#include <ck/path.h>



ck::Path::Path(const ck::string &path) { parse(path); }



ck::string ck::Path::canonicalize(const ck::string &path) {
  // NOTE: We never allow an empty m_string, if it's empty, we just set it to '.'.
  if (path.is_empty()) return ".";

  // NOTE: If there are no dots, no '//' and the path doesn't end with a slash, it is already canonical.
  if (!path.contains(".") && !path.contains("//") && !path.ends_with('/')) return path;

  // canonicalize!
  auto is_absolute = path[0] == '/';
  auto parts = path.split('/');
  size_t approximate_canonical_length = 0;
  ck::vec<ck::string> canonical_parts;

  for (auto &part : parts) {
    if (part == ".") continue;
    if (part == "..") {
      if (canonical_parts.is_empty()) {
        if (is_absolute) {
          // At the root, .. does nothing.
          continue;
        }
      } else {
        if (canonical_parts.last() != "..") {
          // A .. and a previous non-.. part cancel each other.
          canonical_parts.take_last();
          continue;
        }
      }
    }
    approximate_canonical_length += part.size() + 1;
    canonical_parts.push(part);
  }

  if (canonical_parts.is_empty() && !is_absolute) canonical_parts.push(".");

  ck::string out;
  for (int i = 0; i < canonical_parts.size(); i++) {
    if (i > 0 || is_absolute) out += '/';
    out += canonical_parts[i];
  }

  return out;
}


int ck::Path::stat(struct stat *stat) { return lstat(m_string.get(), stat); }

bool ck::Path::parse(const ck::string &path) {
  m_string = canonicalize(path);



  m_parts = m_string.split('/');


  int last_slash_index = -1;
  int last_dot_index = -1;
  for (int i = 0; i < m_string.size(); i++) {
    if (m_string[i] == '/') last_slash_index = i;
  }

  if (last_slash_index != -1) {
    // The path contains a single part and is not absolute. m_dirname = "."sv
    m_dirname = ".";
  } else if (last_slash_index == 0) {
    // The path contains a single part and is absolute. m_dirname = "/"sv
    m_dirname = m_string.substring(0, 1);
  } else {
    m_dirname = m_string.substring(0, last_slash_index);
  }

  if (m_string == "/")
    m_basename = m_string;
  else {
    assert(m_parts.size() > 0);
    m_basename = m_parts.last();
  }

  for (int i = 0; i < m_basename.size(); i++) {
    if (m_basename[i] == '.') last_dot_index = i;
  }


  // NOTE: if the dot index is 0, this means we have ".foo", it's not an extension, as the title would then be "".
  if (last_dot_index != -1 && last_dot_index != 0) {
    m_title = m_basename.substring(0, last_dot_index);
    m_extension = m_basename.substring(last_dot_index + 1, m_basename.size());
  } else {
    m_title = m_basename;
    m_extension = {};
  }
  printf("given:     %s\n", path.get());
  printf("canonical: %s\n", m_string.get());
  printf("last dot: %d\n", last_dot_index);
  printf("basename: %s\n", m_basename.get());
  printf("extension: %s\n", m_title.get());
  printf("extension: %s\n", m_extension.get());

  return true;
}
