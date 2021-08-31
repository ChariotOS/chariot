#include <ck/resource.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// the "resource" here is a section that is just a tar file
extern "C" char __resources_start[];
extern "C" char __resources_end[];

static size_t rsize(void) { return (off_t)__resources_end - (off_t)__resources_start; }


struct resource_ptr {
  const char* name;
  void* data;
  size_t size;
};

ck::option<ck::pair<void*, size_t>> ck::resource::get(const char* name) {
  size_t res_size = rsize();

  if (res_size > 0) {
    size_t n = res_size / sizeof(struct resource_ptr);
    auto* res = (struct resource_ptr*)__resources_start;

    for (int i = 0; i < n; i++) {
      if (!strcmp(name, res[i].name)) {
        return ck::pair(res[i].data, res[i].size);
      }
    }
  }

  return None;
}
