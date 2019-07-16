#pragma once

#ifndef __MOBO_KVMDRIVER_
#define __MOBO_KVMDRIVER_

#include <stdlib.h>
#include <vector>
#include "./machine.h"
#include <linux/kvm.h>

namespace mobo {

struct kvm_vcpu {
  int cpufd = -1;
  int index = 0;
  struct kvm_run *kvm_run;

  void dump_state(FILE*, char *mem = nullptr);
};

class kvmdriver : public driver {
 private:
  void *mem;
  size_t memsize;

  int kvmfd;
  int vmfd;
  int ncpus;
  std::vector<kvm_vcpu> cpus;
  void init_cpus(void);

 public:
  kvmdriver(int kvmfd, int ncpus);
  virtual ~kvmdriver(void);


  virtual void load_elf(std::string &);

  virtual void attach_memory(size_t, void *);
  virtual void run(void);
};
}  // namespace mobo

#endif
