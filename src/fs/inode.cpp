#include <fs/filesystem.h>
#include <fs/inode.h>

fs::inode::inode(fs::filesystem &fs, u32 index) : m_fs(fs), m_index(index) {}
fs::inode::~inode(void) {}

