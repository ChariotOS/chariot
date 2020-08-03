#pragma once


#include <fs.h>
#include <mm.h>
#include <vec.h>

namespace tmp {

  // private data per inode
  struct file {
    /* The data pages */
    vec<ref<mm::page>> pages;
  };



  extern fs::file_operations fops;
  extern fs::dir_operations dops;



  /*
   * Directory Operations
   */
  namespace dop {}


}  // namespace tmp

