#include <assert.h>
#include <mobo/kvm.h>
#include <stdio.h>
#include <stdexcept>

#include <capstone/capstone.h>
#include <fcntl.h>
#include <inttypes.h>  // PRIx64
#include <linux/kvm.h>
#include <mobo/memwriter.h>
#include <mobo/multiboot.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <elfio/elfio.hpp>
#include <iostream>
#include <chrono>

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

mobo::kvm::~kvm() {
  // need to close the vmfd, and all cpu fds
  for (auto cpu : cpus) {
    close(cpu.cpufd);
  }
  close(vmfd);
  munmap(mem, memsize);

}

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

// TODO: abstract the loading of an elf file to a general function that takes
//       some memory and a starting vcpu
void kvm::load_elf(std::string file) {
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

  bool found_multiboot = false;
  struct multiboot_header mbhdr;
  // map the file into memory and attempt to parse a multiboot2_hdr from it
  {
    int fd = open(file.c_str(), O_RDONLY);

    // die if the file is not found
    if (fd == -1)
      throw std::runtime_error("file not found when loading the elf");

    struct stat sb;
    fstat(fd, &sb);
    auto size = sb.st_size;

    char *bin = (char *)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    u32 search_len = std::min((size_t)size, (size_t)MULTIBOOT_SEARCH);

    // arbitrarially on the 5th page
    u64 mbd = 0x5000;

    // now scan the binary for the multiboot2 header
    for (int i = 0; i < search_len; i += MULTIBOOT_HEADER_ALIGN) {
      u32 val = *(u32 *)(bin + i);
      if (val != MULTIBOOT2_HEADER_MAGIC) continue;

      // we found a thing that looks like a multiboot 2 header,
      // check the checksum to make sure it is a real header
      memcpy(&mbhdr, bin + i, sizeof(mbhdr));

      u32 checksum = -(mbhdr.magic + mbhdr.architecture + mbhdr.header_length);
      if (checksum != mbhdr.checksum) continue;

      found_multiboot = true;
      break;
    }

    if (!found_multiboot)
      throw std::runtime_error("no multiboot2 header found!");

    // printf("magic:    %08x\n", mbhdr.magic);
    // printf("arch:     %u\n", mbhdr.arch);
    // printf("length:   %u\n", mbhdr.length);

    if (mbhdr.architecture != 0)
      throw std::runtime_error("only i386 multiboot arch is supported\n");

    // setup the memory information at the mbd
    {
      // create a memory writer to the part of memory that represents the mbd
      memwriter hdr(memory + mbd);

      hdr.write<u32>(0);
      hdr.write<u32>(0);

      struct multiboot_tag_mmap tag;
      tag.type = MULTIBOOT_TAG_TYPE_MMAP;
      tag.entry_size = sizeof(struct multiboot_mmap_entry);

      std::vector<struct multiboot_mmap_entry> entries;

      for (auto bank : this->ram) {
        struct multiboot_mmap_entry entry;

        entry.addr = bank.guest_phys_addr;
        entry.len = bank.size;
        entry.type = MULTIBOOT_MEMORY_AVAILABLE;
        entry.zero = 0;
        entries.push_back(entry);
      }

      tag.size =
          sizeof(struct multiboot_tag_mmap) + (tag.entry_size * entries.size());
      hdr.write(tag);
      for (auto e : entries) {
        hdr.write(e);
      }

      // write the size
      *(u32 *)(memory + mbd) = hdr.get_written();
    }

    // setup general purpose registers
    {
      struct regs r;
      cpus[0].read_regs(r);
      r.rax = 0x36d76289;
      r.rbx = mbd;  // TODO: The physical address of the multiboot information

      r.rip = entry;

      r.rflags &= ~(1 << 17);
      r.rflags &= ~(1 << 9);

      cpus[0].write_regs(r);
    }

    // setup the special registers
    {
      sregs sr;
      cpus[0].read_sregs(sr);

      auto init_seg = [](mobo::segment &seg) {
        seg.present = 1;
        seg.base = 0;
        seg.db = 1;
        seg.g = 1;
        seg.selector = 0x10;
        seg.limit = 0xFFFFFFF;
        seg.type = 0x3;
      };

      init_seg(sr.cs);
      sr.cs.selector = 0x08;
      sr.cs.type = 0x0a;
      sr.cs.s = 1;
      init_seg(sr.ds);
      init_seg(sr.es);
      init_seg(sr.fs);
      init_seg(sr.gs);
      init_seg(sr.ss);

      // clear bit 31, set bit 0
      sr.cr0 &= ~(1 << 31);
      sr.cr0 |= (1 << 0);

      cpus[0].write_sregs(sr);
    }

    // unmap the memory and close the fd
    munmap(bin, size);
    close(fd);
  }
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
  this->mem = mmap(NULL, nbytes, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  this->memsize = nbytes;

  // Setup the PCI hole if needed... Basically map the region of memory
  // differently if it needs to have a hole in it. The memory is still just one
  // region of memory, except we seperate the region using kvm's
  // USER_MEMORY_REGION system
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

// #define RECORD_VMEXIT_LIFETIME

void kvm::run(void) {
  auto &cpufd = cpus[0].cpufd;
  auto run = cpus[0].kvm_run;

  mobo::regs regs;

  cpus[0].read_regs(regs);

  while (1) {
#ifdef RECORD_VMEXIT_LIFETIME
    auto start = std::chrono::high_resolution_clock::now();
#endif
    ioctl(cpufd, KVM_RUN, NULL);
#ifdef RECORD_VMEXIT_LIFETIME
    auto end = std::chrono::high_resolution_clock::now();

    auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    printf("ran for %lu us\n", dur.count());
#endif

    // printf("Exited: %s\n", kvm_exit_reasons[run->exit_reason]);
    int stat = run->exit_reason;

    if (stat == KVM_EXIT_MMIO) {
      printf("[MMIO] ");
      void *addr = (void *)run->mmio.phys_addr;
      if (run->mmio.is_write) {
        printf("write %p\n", addr);
      } else {
        printf("read  %p\n", addr);
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
      dev_mgr.handle_io(
          nullptr, run->io.port, run->io.direction == KVM_EXIT_IO_IN,
          (void *)((char *)run + run->io.data_offset), run->io.size);
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

    if (stat == KVM_EXIT_FAIL_ENTRY) {
      printf("failed to enter: %llu\n",
             run->fail_entry.hardware_entry_failure_reason);
      return;
    }

    cpus[0].read_regs(regs);

    printf("unhandled exit: %d at rip = %p\n", run->exit_reason,
           (void *)regs.rip);
    break;
  }

  // cpus[0].dump_state(stdout, (char *)this->mem);
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
                               mobo::segment const &seg) {
  fprintf(out, "%-3s=%04x %016" PRIx64 " %08x %d %02x %02x  %02x\n", name,
          seg.selector, (size_t)seg.base, seg.limit, seg.present, seg.db,
          seg.dpl, seg.type);
}

void kvm_vcpu::dump_state(FILE *out, char *mem) {
  mobo::regs regs;
  read_regs(regs);

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

  mobo::sregs sregs;
  read_sregs(sregs);

  fprintf(out, "    sel  base             limit    p db dpl type\n");
  cpu_dump_seg_cache(this, out, "ES", sregs.es);
  cpu_dump_seg_cache(this, out, "CS", sregs.cs);
  cpu_dump_seg_cache(this, out, "SS", sregs.ss);
  cpu_dump_seg_cache(this, out, "DS", sregs.ds);
  cpu_dump_seg_cache(this, out, "FS", sregs.fs);
  cpu_dump_seg_cache(this, out, "GS", sregs.gs);
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

  fpu_regs fpu;
  read_fregs(fpu);

  {
    int written = 0;
    for (int i = 0; i < 16; i++) {
      fprintf(out, "XMM%-2d=", i);

      for (int j = 0; j < 16; j++) {
        fprintf(out, "%02x", fpu.xmm[i][j]);
      }
      written++;

      if (written == 2) {
        fprintf(out, "\n");
        written = 0;
      } else {
        fprintf(out, " ");
      }
    }
  }

  fprintf(out, "\n");

  return;
  if (mem != nullptr) {
    printf("Code:\n");
    csh handle;
    cs_insn *insn;
    size_t count;

    uint8_t *code = (uint8_t *)mem + regs.rip;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return;
    count = cs_disasm(handle, code, -1, regs.rip, 0, &insn);
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

void kvm_vcpu::read_regs(regs &r) {
  struct kvm_regs regs;
  ioctl(cpufd, KVM_GET_REGS, &regs);

  r.rax = regs.rax;
  r.rbx = regs.rbx;
  r.rcx = regs.rcx;
  r.rdx = regs.rdx;

  r.rsi = regs.rsi;
  r.rdi = regs.rdi;
  r.rsp = regs.rsp;
  r.rbp = regs.rbp;

  r.r8 = regs.r8;
  r.r9 = regs.r8;
  r.r10 = regs.r10;
  r.r11 = regs.r11;
  r.r12 = regs.r12;
  r.r13 = regs.r13;
  r.r14 = regs.r14;
  r.r15 = regs.r15;

  r.rip = regs.rip;
  r.rflags = regs.rflags;
}

void kvm_vcpu::write_regs(regs &regs) {
  struct kvm_regs r;
  r.rax = regs.rax;
  r.rbx = regs.rbx;
  r.rcx = regs.rcx;
  r.rdx = regs.rdx;

  r.rsi = regs.rsi;
  r.rdi = regs.rdi;
  r.rsp = regs.rsp;
  r.rbp = regs.rbp;

  r.r8 = regs.r8;
  r.r9 = regs.r8;
  r.r10 = regs.r10;
  r.r11 = regs.r11;
  r.r12 = regs.r12;
  r.r13 = regs.r13;
  r.r14 = regs.r14;
  r.r15 = regs.r15;

  r.rip = regs.rip;
  r.rflags = regs.rflags;
  ioctl(cpufd, KVM_SET_REGS, &r);
}

void kvm_vcpu::read_sregs(sregs &r) {
  struct kvm_sregs sr;

  ioctl(cpufd, KVM_GET_SREGS, &sr);

  static auto copy_segment = [](auto &dst, auto &src) {
    dst.base = src.base;
    dst.limit = src.limit;
    dst.selector = src.selector;
    dst.type = src.type;
    dst.present = src.present;
    dst.dpl = src.dpl;
    dst.db = src.db;
    dst.s = src.s;
    dst.l = src.l;
    dst.g = src.g;
    dst.avl = src.avl;
    dst.unusable = src.unusable;
    // dont need to copy padding
  };

  copy_segment(r.cs, sr.cs);
  copy_segment(r.ds, sr.ds);
  copy_segment(r.es, sr.es);
  copy_segment(r.fs, sr.fs);
  copy_segment(r.gs, sr.gs);
  copy_segment(r.ss, sr.ss);
  copy_segment(r.tr, sr.tr);

  r.gdt.base = sr.gdt.base;
  r.gdt.limit = sr.gdt.limit;

  r.idt.base = sr.idt.base;
  r.idt.limit = sr.idt.limit;

  r.cr0 = sr.cr0;
  r.cr2 = sr.cr2;
  r.cr3 = sr.cr3;
  r.cr4 = sr.cr4;
  r.cr8 = sr.cr8;

  r.efer = sr.efer;
  r.apic_base = sr.apic_base;
  for (int i = 0; i < (NR_INTERRUPTS + 63) / 64; i++) {
    r.interrupt_bitmap[i] = sr.interrupt_bitmap[i];
  }
}
void kvm_vcpu::write_sregs(sregs &sr) {
  struct kvm_sregs r;

  static auto copy_segment = [](auto &dst, auto &src) {
    dst.base = src.base;
    dst.limit = src.limit;
    dst.selector = src.selector;
    dst.type = src.type;
    dst.present = src.present;
    dst.dpl = src.dpl;
    dst.db = src.db;
    dst.s = src.s;
    dst.l = src.l;
    dst.g = src.g;
    dst.avl = src.avl;
    dst.unusable = src.unusable;
    // dont need to copy padding
  };

  copy_segment(r.cs, sr.cs);
  copy_segment(r.ds, sr.ds);
  copy_segment(r.es, sr.es);
  copy_segment(r.fs, sr.fs);
  copy_segment(r.gs, sr.gs);
  copy_segment(r.ss, sr.ss);
  copy_segment(r.tr, sr.tr);

  r.gdt.base = sr.gdt.base;
  r.gdt.limit = sr.gdt.limit;

  r.idt.base = sr.idt.base;
  r.idt.limit = sr.idt.limit;

  r.cr0 = sr.cr0;
  r.cr2 = sr.cr2;
  r.cr3 = sr.cr3;
  r.cr4 = sr.cr4;
  r.cr8 = sr.cr8;

  r.efer = sr.efer;
  r.apic_base = sr.apic_base;
  for (int i = 0; i < (NR_INTERRUPTS + 63) / 64; i++) {
    r.interrupt_bitmap[i] = sr.interrupt_bitmap[i];
  }

  ioctl(cpufd, KVM_SET_SREGS, &sr);
}

void kvm_vcpu::read_fregs(fpu_regs &dst) {
  struct kvm_fpu src;

  ioctl(cpufd, KVM_GET_FPU, &src);

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 16; j++) {
      dst.fpr[i][j] = src.fpr[i][j];
    }
  }

  dst.fcw = src.fcw;
  dst.fsw = src.fsw;
  dst.ftwx = src.ftwx;
  dst.last_opcode = src.last_opcode;
  dst.last_ip = src.last_ip;
  dst.last_dp = src.last_dp;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      dst.xmm[i][j] = src.xmm[i][j];
    }
  }
  dst.mxcsr = dst.mxcsr;
}
void kvm_vcpu::write_fregs(fpu_regs &src) {
  struct kvm_fpu dst;

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 16; j++) {
      dst.fpr[i][j] = src.fpr[i][j];
    }
  }

  dst.fcw = src.fcw;
  dst.fsw = src.fsw;
  dst.ftwx = src.ftwx;
  dst.last_opcode = src.last_opcode;
  dst.last_ip = src.last_ip;
  dst.last_dp = src.last_dp;
  for (int i = 0; i < 16; i++) {
    for (int j = 0; j < 16; j++) {
      dst.xmm[i][j] = src.xmm[i][j];
    }
  }
  dst.mxcsr = dst.mxcsr;

  ioctl(cpufd, KVM_SET_FPU, &dst);
}

