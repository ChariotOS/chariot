#pragma once

#include <ck/ptr.h>
#include <dev/hardware.h>
#include <lock.h>

namespace fs {

  class Node;

  class FileSystem : public ck::refcounted<FileSystem> {
   public:
    dev_t dev;
    long block_size;
    long max_filesize;
    ck::ref<fs::Node> root;
    ck::string arguments;
    rwlock lock;
  };

}  // namespace fs