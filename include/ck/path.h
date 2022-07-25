#pragma once

#include <ck/string.h>
#include <ck/vec.h>

#include <sys/stat.h>

namespace ck {
  class Path {
   public:
    Path(void) = default;
    Path(const ck::string &path);

    ~Path(void) = default;

    // parse a string into this path, returning success or not (Will almost always succeed)
    bool parse(const ck::string &path);

    // Is this path absolute?
    bool absolute() const { return m_string.size() > 0 && m_string[0] == '/'; }

		// Get the stat for the path
		int stat(struct stat *stat);

    static ck::string canonicalize(const ck::string &s);

   private:
    ck::string m_string;          // the whole thing
    ck::vec<ck::string> m_parts;  // the individual parts
		// These do NOT need to be this inefficient :)
		ck::string m_dirname;
		ck::string m_basename;
		ck::string m_title;
		ck::string m_extension;
  };
}  // namespace ck
