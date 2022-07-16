// #include <kctl.h>
#include <syscall.h>
#include <kctl.h>
#include <ck/option.h>

#include <kctl_node.h>

// kctl hw.ncpus
//   -> "4"


class HardwareNode : public kctl::Node {
 public:
  bool kctl_read(kctl::Path path, ck::string &out) override {
    if (!path) return false;

    if (path[0] == KCTL_NCPUS) {
      out.appendf("%d", cpu::nproc());
      return true;
    }

    return false;
  }
};



class ProcNode : public kctl::Node {
 public:
  bool kctl_read(kctl::Path path, ck::string &out) override {
    if (!path) return false;

    if (path.is_leaf()) {
      // format all the pids into a list
      //
      //
      return 0;
    }

    return false;
  }
};


extern bool hacky_proc_kctl_read(kctl::Path path, ck::string &out);
static ck::string kctl_tostring(off_t *namepath, unsigned namelen) {
  ck::string s;

  for (int i = 0; i < namelen; i++) {
    if (i != 0) s += '.';
    off_t name = namepath[i];
    if (KCTL_IS_NAME(name)) {
#define __KCTL(number, _name, uppername) \
  if (name == number) {                  \
    s += #_name;                         \
    continue;                            \
  }
      ENUMERATE_KCTL_NAMES
#undef __KCTL
    }


    s.appendf("%lu", name);
  }

  return s;
}


static HardwareNode node_hw;

int sys::kctl(off_t *_name, unsigned namelen, char *oval, size_t *olen, char *nval, size_t nlen) {
  // Validate pointers
  if (oval != NULL && !VALIDATE_RD(_name, namelen * sizeof(off_t))) return -EINVAL;
  if (nval != NULL && !VALIDATE_RD(nval, nlen)) return -EINVAL;
  // Validate that the name is not too long
  if (namelen > KCTL_MAX_NAMELEN) return -ENAMETOOLONG;
  // Copy the name path to the kernel
  off_t name[KCTL_MAX_NAMELEN];

  memcpy(name, _name, namelen * sizeof(off_t));


  kctl::Path path(name, namelen);

  bool success = false;
  ck::string read_out;
  switch (path[0]) {
    case KCTL_HW:
      success = node_hw.kctl_read(path.next(), read_out);
      break;
    case KCTL_PROC:
      success = hacky_proc_kctl_read(path.next(), read_out);
      break;
  }

  auto s = kctl_tostring(name, namelen);
  printf("kctl: %s = '%s'\n", s.get(), read_out.get());
  // TODO: validate oval if
  // if (!VALIDATE_RD(oval, namelen * sizeof(int))) return -EINVAL;
  return 0;
}
