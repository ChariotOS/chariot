#pragma once

#ifndef __MOBO_KVMDRIVER_
#define __MOBO_KVMDRIVER_

#include <linux/kvm.h>
#include <mobo/dev_mgr.h>
#include <mobo/vcpu.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

/*
 * The hole includes VESA framebuffer and PCI memory.
 */
#define KVM_32BIT_MAX_MEM_SIZE (1ULL << 32)
#define KVM_32BIT_GAP_SIZE (768 << 20)
#define KVM_32BIT_GAP_START (KVM_32BIT_MAX_MEM_SIZE - KVM_32BIT_GAP_SIZE)

#define KVM_MMIO_START KVM_32BIT_GAP_START

namespace mobo {

struct kvm_vcpu : public mobo::vcpu {
  int cpufd = -1;
  int index = 0;

  char *mem;
  size_t memsize;

  struct kvm_run *kvm_run;
  size_t run_size = 0;

  struct kvm_regs initial_regs;
  struct kvm_sregs initial_sregs;
  struct kvm_fpu initial_fpu;


struct kvm_coalesced_mmio_ring *ring;


  // GPR
  virtual void read_regs(regs &);
  virtual void write_regs(regs &);
  // SPR
  virtual void read_sregs(sregs &);
  virtual void write_sregs(sregs &);
  // FPR
  virtual void read_fregs(fpu_regs &);
  virtual void write_fregs(fpu_regs &);

  virtual void dump_state(FILE *, char *mem = nullptr);

  virtual void *translate_address(u64 gva);
  virtual void reset(void);
};

// a memory bank represents a segment of memory in the kvm CPU
struct ram_bank {
  uint64_t guest_phys_addr;
  void *host_addr;
  size_t size;
};

class kvm {
 private:
  void *mem;
  size_t memsize;

  int kvmfd;
  int vmfd;
  int ncpus;
  std::vector<kvm_vcpu> cpus;
  device_manager dev_mgr;
  void init_cpus(void);

  // ram is made up of a series of banks, typically the region
  // before the PCI gap, and the region after the gap
  std::vector<ram_bank> ram;
  void attach_bank(ram_bank &&);

 public:

  bool halted = false;
  bool shutdown = false;

  kvm(int kvmfd, int ncpus);
  ~kvm(void);
  void load_elf(std::string);

  void init_ram(size_t);
  void run(void);


  void handle_coalesced_mmio(kvm_vcpu &cpu);


  bool emulate_mmio(kvm_vcpu &cpu, u64 phys_addr, u8 *data, u32 len, bool is_write);

  void reset();
};

}  // namespace mobo

#endif