#include <fs/file.h>
#include <fs/filesystem.h>

// auto incrementing id for every filesystem. Used to uniquely identify a
// filesystem quickly. Though this does mean there can only ever be 2^32
// mounted filesystems ever... oh well
static u32 next_fsid = 0;

fs::filesystem::filesystem(void) {
}

fs::filesystem::~filesystem(void) {}

