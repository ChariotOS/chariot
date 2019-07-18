#include <assert.h>
#include <mobo/kvm.h>
#include <stdio.h>
#include <stdexcept>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <inttypes.h>  // PRIx64
#include <linux/kvm.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <elfio/elfio.hpp>
#include <iostream>

using namespace mobo;

#define DEFINE_KVM_EXIT_REASON(reason) [reason] = #reason

const char *kvm_exit_reasons[] = {
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_UNKNOWN),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_EXCEPTION),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_IO),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_HYPERCALL),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_DEBUG),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_HLT),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_MMIO),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_IRQ_WINDOW_OPEN),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_SHUTDOWN),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_FAIL_ENTRY),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTR),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_SET_TPR),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_TPR_ACCESS),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_SIEIC),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_S390_RESET),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_DCR),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_NMI),
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_INTERNAL_ERROR),
#ifdef CONFIG_PPC64
    DEFINE_KVM_EXIT_REASON(KVM_EXIT_PAPR_HCALL),
#endif
};
#undef DEFINE_KVM_EXIT_REASON

struct cpuid_regs {
  int eax;
  int ebx;
  int ecx;
  int edx;
};
#define MAX_KVM_CPUID_ENTRIES 100
static void filter_cpuid(struct kvm_cpuid2 *);

kvm::kvm(int kvmfd, int ncpus) : kvmfd(kvmfd), ncpus(ncpus) {
  assert(ncpus == 1);  // for now...

  int ret;

  ret = ioctl(kvmfd, KVM_GET_API_VERSION, NULL);
  if (ret == -1) throw std::runtime_error("KVM_GET_API_VERSION");
  if (ret != 12) throw std::runtime_error("KVM_GET_API_VERSION invalid");

  ret = ioctl(kvmfd, KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY);
  if (ret == -1) throw std::runtime_error("KVM_CHECK_EXTENSION");
  if (!ret)
    throw std::runtime_error(
        "Required extension KVM_CAP_USER_MEM not available");

  // Next, we need to create a virtual machine (VM), which represents everything
  // associated with one emulated system, including memory and one or more CPUs.
  // KVM gives us a handle to this VM in the form of a file descriptor:
  vmfd = ioctl(kvmfd, KVM_CREATE_VM, (unsigned long)0);

  init_cpus();
}

mobo::kvm::~kvm() {}

void kvm::init_cpus(void) {
  int kvm_run_size = ioctl(kvmfd, KVM_GET_VCPU_MMAP_SIZE, nullptr);

  // get cpuid info
  struct kvm_cpuid2 *kvm_cpuid;

  kvm_cpuid = (kvm_cpuid2 *)calloc(
      1,
      sizeof(*kvm_cpuid) + MAX_KVM_CPUID_ENTRIES * sizeof(*kvm_cpuid->entries));
  kvm_cpuid->nent = MAX_KVM_CPUID_ENTRIES;
  if (ioctl(kvmfd, KVM_GET_SUPPORTED_CPUID, kvm_cpuid) < 0)
    throw std::runtime_error("KVM_GET_SUPPORTED_CPUID failed");

  filter_cpuid(kvm_cpuid);

  for (int i = 0; i < ncpus; i++) {
    int vcpufd = ioctl(vmfd, KVM_CREATE_VCPU, nullptr);

    // init the cpuid
    if (ioctl(vcpufd, KVM_SET_CPUID2, kvm_cpuid) < 0)
      throw std::runtime_error("KVM_SET_CPUID2 failed");

    auto *run = (struct kvm_run *)mmap(
        NULL, kvm_run_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpufd, 0);

    struct kvm_regs regs = {
        .rflags = 0x2,
    };
    ioctl(vcpufd, KVM_SET_REGS, &regs);

    struct kvm_sregs sregs;
    ioctl(vcpufd, KVM_GET_SREGS, &sregs);
    sregs.cs.base = 0;
    ioctl(vcpufd, KVM_SET_SREGS, &sregs);

    kvm_vcpu cpu;
    cpu.cpufd = vcpufd;
    cpu.kvm_run = run;
    cpus.push_back(cpu);
  }

  free(kvm_cpuid);
}

void kvm::load_elf(std::string &file) {
  char *memory = (char *)mem;  // grab a char buffer reference to the mem

  ELFIO::elfio reader;
  reader.load(file);


  auto entry = reader.get_entry();

  auto secc = reader.sections.size();
  for (int i = 0; i < secc; i++) {
    auto psec = reader.sections[i];
    auto type = psec->get_type();
    if (psec->get_name() == ".comment") continue;

    if (type == SHT_PROGBITS) {
      auto size = psec->get_size();
      if (size == 0) continue;
      const char *data = psec->get_data();
      char *dst = memory + psec->get_address();
      memcpy(dst, data, size);
    }
  }

  // set the cpu0.rip to the entrypoint parsed from the elf
  auto cpufd = cpus[0].cpufd;
  struct kvm_regs regs;
  ioctl(cpufd, KVM_GET_REGS, &regs);
  regs.rip = entry;
  ioctl(cpufd, KVM_SET_REGS, &regs);
}


void kvm::attach_bank(ram_bank &&bnk) {
  struct kvm_userspace_memory_region code_region = {
      .slot = (uint32_t)ram.size(),
      .guest_phys_addr = bnk.guest_phys_addr,
      .memory_size = bnk.size,
      .userspace_addr = (uint64_t)bnk.host_addr,
  };
  ioctl(vmfd, KVM_SET_USER_MEMORY_REGION, &code_region);
  ram.push_back(std::move(bnk));
}


void kvm::init_ram(size_t nbytes) {
  this->mem =mmap(NULL, nbytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
              -1, 0);
  this->memsize = nbytes;

  if (nbytes < KVM_32BIT_GAP_START) {
    ram_bank bnk;
    bnk.guest_phys_addr = 0x0000;
    bnk.size = nbytes;
    bnk.host_addr = this->mem;
    attach_bank(std::move(bnk));
  } else {
    // we need to break up the ram to support the PCI Hole
    ram_bank bnk1;
    bnk1.host_addr = this->mem;
    bnk1.guest_phys_addr = 0x0000;
    bnk1.size = KVM_32BIT_GAP_START;
    attach_bank(std::move(bnk1));

    /* Second RAM range from 4GB to the end of RAM: */
    ram_bank bnk2;
    bnk2.guest_phys_addr = KVM_32BIT_MAX_MEM_SIZE;
    bnk2.host_addr = (char *)this->mem + bnk2.guest_phys_addr;
    bnk2.size = nbytes - bnk2.guest_phys_addr;
    attach_bank(std::move(bnk2));
  }
}

void kvm::run(void) {
  auto &cpufd = cpus[0].cpufd;
  auto run = cpus[0].kvm_run;

  struct kvm_regs regs;

  ioctl(cpufd, KVM_GET_REGS, &regs);

  while (1) {
    ioctl(cpufd, KVM_RUN, NULL);

    // cpus[0].dump_state(stderr);

    // printf("Exited: %s\n", kvm_exit_reasons[run->exit_reason]);
    int stat = run->exit_reason;
    if (stat == KVM_EXIT_MMIO) {
      void *addr = (void *)run->mmio.phys_addr;
      if (run->mmio.is_write) {
        printf("mmio write %p\n", addr);
      } else {
        printf("mmio read  %p\n", addr);
      }
      continue;
    }

    if (stat == KVM_EXIT_SHUTDOWN) {
      printf("SHUTDOWN (probably a triple fault, lol)\n");

      cpus[0].dump_state(stderr, (char *)this->mem);
      return;
      break;
    }

    if (stat == KVM_EXIT_HLT) {
      break;
    }

    if (stat == KVM_EXIT_IO) {
      dev_mgr.handle_io(run->io.port, run->io.direction == KVM_EXIT_IO_IN,
                        (void *)((char *)run + run->io.data_offset),
                        run->io.size);
      continue;
    }

    if (stat == KVM_EXIT_INTERNAL_ERROR) {
      if (run->internal.suberror == KVM_INTERNAL_ERROR_EMULATION) {
        fprintf(stderr, "emulation failure\n");
        cpus[0].dump_state(stderr);
        return;
      }
      printf("kvm internal error: suberror %d\n", run->internal.suberror);
      return;
    }

    ioctl(cpufd, KVM_GET_REGS, &regs);

    printf("unhandled exit: %d at rip = %p\n", run->exit_reason,
           (void *)regs.rip);
    break;
  }

  cpus[0].dump_state(stdout);
}

static inline void host_cpuid(struct cpuid_regs *regs) {
  __asm__ volatile("cpuid"
                   : "=a"(regs->eax), "=b"(regs->ebx), "=c"(regs->ecx),
                     "=d"(regs->edx)
                   : "0"(regs->eax), "2"(regs->ecx));
}

static void filter_cpuid(struct kvm_cpuid2 *kvm_cpuid) {
  unsigned int i;
  struct cpuid_regs regs;

  /*
   * Filter CPUID functions that are not supported by the hypervisor.
   */
  for (i = 0; i < kvm_cpuid->nent; i++) {
    struct kvm_cpuid_entry2 *entry = &kvm_cpuid->entries[i];

    switch (entry->function) {
      case 0:

        regs = (struct cpuid_regs){
            .eax = 0x00,

        };
        host_cpuid(&regs);
        /* Vendor name */
        entry->ebx = regs.ebx;
        entry->ecx = regs.ecx;
        entry->edx = regs.edx;
        break;
      case 1:
        /* Set X86_FEATURE_HYPERVISOR */
        if (entry->index == 0) entry->ecx |= (1 << 31);
        /* Set CPUID_EXT_TSC_DEADLINE_TIMER*/
        if (entry->index == 0) entry->ecx |= (1 << 24);
        break;
      case 6:
        /* Clear X86_FEATURE_EPB */
        entry->ecx = entry->ecx & ~(1 << 3);
        break;
      case 10: { /* Architectural Performance Monitoring */
        union cpuid10_eax {
          struct {
            unsigned int version_id : 8;
            unsigned int num_counters : 8;
            unsigned int bit_width : 8;
            unsigned int mask_length : 8;
          } split;
          unsigned int full;
        } eax;

        /*
         * If the host has perf system running,
         * but no architectural events available
         * through kvm pmu -- disable perf support,
         * thus guest won't even try to access msr
         * registers.
         */
        if (entry->eax) {
          eax.full = entry->eax;
          if (eax.split.version_id != 2 || !eax.split.num_counters)
            entry->eax = 0;
        }
        break;
      }
      default:
        /* Keep the CPUID function as -is */
        break;
    };
  }
}

/* eflags masks */
#define CC_C 0x0001
#define CC_P 0x0004
#define CC_A 0x0010
#define CC_Z 0x0040
#define CC_S 0x0080
#define CC_O 0x0800

#define TF_SHIFT 8
#define IOPL_SHIFT 12
#define VM_SHIFT 17

#define TF_MASK 0x00000100
#define IF_MASK 0x00000200
#define DF_MASK 0x00000400
#define IOPL_MASK 0x00003000
#define NT_MASK 0x00004000
#define RF_MASK 0x00010000
#define VM_MASK 0x00020000
#define AC_MASK 0x00040000
#define VIF_MASK 0x00080000
#define VIP_MASK 0x00100000
#define ID_MASK 0x00200000

static void cpu_dump_seg_cache(kvm_vcpu *cpu, FILE *out, const char *name,
                               struct kvm_segment const &seg) {
  fprintf(out, "%-3s=%04x %016" PRIx64 " %08x ", name, seg.selector,
          (size_t)seg.base, seg.limit);

  fprintf(out, "\n");
}

void kvm_vcpu::dump_state(FILE *out, char *mem) {
  struct kvm_regs regs;
  ioctl(cpufd, KVM_GET_REGS, &regs);

  unsigned int eflags = regs.rflags;
#define GET(name) \
  *(uint64_t *)(((char *)&regs) + offsetof(struct kvm_regs, name))

#define REGFMT "%016" PRIx64
  fprintf(out,
          "RAX=" REGFMT " RBX=" REGFMT " RCX=" REGFMT " RDX=" REGFMT
          "\n"
          "RSI=" REGFMT " RDI=" REGFMT " RBP=" REGFMT " RSP=" REGFMT
          "\n"
          "R8 =" REGFMT " R9 =" REGFMT " R10=" REGFMT " R11=" REGFMT
          "\n"
          "R12=" REGFMT " R13=" REGFMT " R14=" REGFMT " R15=" REGFMT
          "\n"
          "RIP=" REGFMT " RFL=%08x [%c%c%c%c%c%c%c]\n",

          GET(rax), GET(rbx), GET(rcx), GET(rdx), GET(rsi), GET(rdi), GET(rbp),
          GET(rsp), GET(r8), GET(r9), GET(r10), GET(r11), GET(r12), GET(r13),
          GET(r14), GET(r15), GET(rip), eflags, eflags & DF_MASK ? 'D' : '-',
          eflags & CC_O ? 'O' : '-', eflags & CC_S ? 'S' : '-',
          eflags & CC_Z ? 'Z' : '-', eflags & CC_A ? 'A' : '-',
          eflags & CC_P ? 'P' : '-', eflags & CC_C ? 'C' : '-');

  struct kvm_sregs sregs;

  ioctl(cpufd, KVM_GET_SREGS, &sregs);

  fprintf(out, "    sel  base             limit\n");
  cpu_dump_seg_cache(this, out, "ES", sregs.cs);
  cpu_dump_seg_cache(this, out, "CS", sregs.cs);
  cpu_dump_seg_cache(this, out, "SS", sregs.cs);
  cpu_dump_seg_cache(this, out, "DS", sregs.cs);
  cpu_dump_seg_cache(this, out, "FS", sregs.cs);
  cpu_dump_seg_cache(this, out, "GS", sregs.cs);
  cpu_dump_seg_cache(this, out, "LDT", sregs.ldt);
  cpu_dump_seg_cache(this, out, "TR", sregs.tr);

  fprintf(out, "GDT=     %016" PRIx64 " %08x\n", (size_t)sregs.gdt.base,
          (int)sregs.gdt.limit);
  fprintf(out, "IDT=     %016" PRIx64 " %08x\n", (size_t)sregs.idt.base,
          (int)sregs.idt.limit);

  fprintf(out,
          "CR0=%016" PRIx64 " CR2=%016" PRIx64 " CR3=%016" PRIx64 " CR4=%08x\n",
          (size_t)sregs.cr0, (size_t)sregs.cr2, (size_t)sregs.cr3,
          (int)sregs.cr4);

  fprintf(out, "EFER=%016" PRIx64 "\n", (size_t)sregs.efer);

  fprintf(out, "\n");

  if (mem != nullptr) {
    printf("Code:\n");
    csh handle;
    cs_insn *insn;
    size_t count;

    uint8_t *code = (uint8_t *)mem + regs.rip;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return;
    count = cs_disasm(handle, code + regs.rip, 100, regs.rip, 0, &insn);
    if (count > 0) {
      size_t j;
      for (j = 0; j < count; j++) {
        if (j > 5) break;
        printf("0x%" PRIx64 ":\t%s\t\t%s\n", insn[j].address, insn[j].mnemonic,
               insn[j].op_str);
      }

      cs_free(insn, count);
    } else
      printf("ERROR: Failed to disassemble given code!\n");

    cs_close(&handle);
  }
#undef GET
}
