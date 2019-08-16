#pragma once

#include <string.h>
#include <ptr.h>
#include <fs/filesystem.h>

/**
 *
 * The virtual file system is the system that allows filesystems to be mounted
 * recursively in the directory tree. It is the main API for generalized
 * filesystem access
 */

namespace fs {
namespace vfs {
  class mount {
    public:
      mount(unique_ptr<fs::filesystem>);
  };
};
};  // namespace fs
