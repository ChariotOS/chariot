#pragma once


#ifndef __DEVFS_H__
#define __DEVFS_H__

#include <fs.h>


namespace devfs {


  /*
   */
  class DirectoryNode : public fs::DirectoryNode {
    using fs::DirectoryNode::DirectoryNode;

		// Explicit entries. All device nodes are included as entries dynamically
		// and do not live in this map. This map is mostly for . and ..
    ck::map<ck::string, ck::box<fs::DirectoryEntry>> entries;

    virtual int touch(ck::string name, fs::Ownership &);
    virtual int mkdir(ck::string name, fs::Ownership &);
    virtual int unlink(ck::string name);
    virtual ck::ref<fs::Node> lookup(ck::string name);
    virtual ck::vec<fs::DirectoryEntry *> dirents(void);
    virtual fs::DirectoryEntry *get_direntry(ck::string name);
    virtual int link(ck::string name, ck::ref<fs::Node> node);
  };


  struct FileSystem : public fs::FileSystem {
    FileSystem(ck::string args, int flags);
    static ck::ref<fs::Node> get_root(void);

    virtual ~FileSystem();

		static ck::ref<fs::FileSystem> mount(ck::string options, int flags, ck::string device);
  };


  void init(void);
};  // namespace devfs

#endif
