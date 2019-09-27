#pragma once


#include <map.h>
#include <fs/filesystem.h>

namespace fs {
class tmp : public fs::filesystem {
  public:
    tmp();
    virtual ~tmp();

    virtual bool init(void);


    virtual vnoderef get_inode(u32 index);
    virtual vnoderef get_root_inode();
    virtual u64 block_size(void);

    virtual bool umount(void);

    vnoderef alloc_node(fs::file_type t, u32 mode);

    bool unlink_node(u32 index);

  private:

    vnoderef m_root;
    u32 m_next_inode = 1;
    map<u32, vnoderef> inodemap;

};
}
