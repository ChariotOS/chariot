#pragma once


#include <fs.h>
#include <mm.h>
#include <ck/vec.h>

namespace tmpfs {


  struct FileNode : public fs::FileNode {
    using fs::FileNode::FileNode;

    virtual int seek(fs::File &, off_t old_off, off_t new_off);
    virtual ssize_t read(fs::File &, char *dst, size_t count);
    virtual ssize_t write(fs::File &, const char *, size_t);
    virtual int resize(fs::File &, size_t);
    virtual int stat(fs::File &, struct stat *);
  };

  struct DirNode : public fs::DirectoryNode {
    using fs::DirectoryNode::DirectoryNode;
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
    /* memory statistics, to keep tmpfs from using too much memory */
    size_t allowed_pages;
    size_t used_pages;
    spinlock lock;
    uint64_t next_inode = 0;


    FileSystem(ck::string args, int flags);

    virtual ~FileSystem();
  };

	void init(void);


}  // namespace tmp
