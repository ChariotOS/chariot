#include <elf/loader.h>
#include <printk.h>

#define round_up(x, y) (((x) + (y)-1) & ~((y)-1))
#define round_down(x, y) ((x) & ~((y)-1))

static const char elf_header[] = {0x7f, 0x45, 0x4c, 0x46};

bool elf::validate(fs::file &fd) {
  Elf64_Ehdr ehdr;
  return elf::validate(fd, ehdr);
}

bool elf::validate(fs::file &fd, Elf64_Ehdr &ehdr) {
  fd.seek(0, SEEK_SET);
  int header_read = fd.read(&ehdr, sizeof(ehdr));

  if (header_read != sizeof(ehdr)) {
    return false;
  }

  for (int i = 0; i < 4; i++) {
    if (ehdr.e_ident[i] != elf_header[i]) {
      return false;
    }
  }

  switch (ehdr.e_machine) {
    case EM_X86_64:
    case EM_ARM:
    case EM_AARCH64:
      break;
    default:
      return false;
      break;
  }

  return true;
}

int elf::load(const char *path, struct process &p, mm::space &mm, ref<fs::file> fd,
              u64 &entry) {
  Elf64_Ehdr ehdr;

  off_t off = 0;


  if (!elf::validate(*fd, ehdr)) {
  	printk("[ELF LOADER] elf not valid\n");
    return -1;
  }

  // the binary is valid, so lets read the headers!
  entry = off + ehdr.e_entry;

  Elf64_Shdr *sec_hdrs = new Elf64_Shdr[ehdr.e_shnum];

  // seek to and read the headers
  fd->seek(ehdr.e_shoff, SEEK_SET);
  auto sec_expected = ehdr.e_shnum * sizeof(*sec_hdrs);
  auto sec_read = fd->read(sec_hdrs, sec_expected);

  if (sec_read != sec_expected) {
    delete[] sec_hdrs;
  	printk("sec_read != sec_expected\n");
    return -1;
  }

  delete[] sec_hdrs;

  auto handle_bss = [&](Elf64_Phdr &ph) -> void {
    if (ph.p_memsz > ph.p_filesz) {
      auto addr = ph.p_vaddr;
      // possibly map a new bss region, anonymously in?
      auto file_end = addr + (ph.p_memsz - ph.p_filesz);
      auto file_page_end = round_up(file_end, PGSIZE);

      auto page_end = round_up(addr + ph.p_memsz, PGSIZE);

      if (page_end > file_page_end) {
        printk("need a new page!\n");
      }
    }
  };

  // read program headers
  Elf64_Phdr *phdr = new Elf64_Phdr[ehdr.e_phnum];
  fd->seek(ehdr.e_phoff, SEEK_SET);
  auto hdrs_size = ehdr.e_phnum * ehdr.e_phentsize;
  auto hdrs_read = fd->read(phdr, hdrs_size);

  if (hdrs_read != hdrs_size) {
    delete[] phdr;
		printk("hdrs_read != hdrs_size\n");
    return -1;
  }

  size_t i = 0;

  // skip to the first loadable header
  while (i < ehdr.e_phnum && phdr[i].p_type != PT_LOAD) i++;

  if (i == ehdr.e_phnum) {
    delete[] phdr;
		printk("i == ehdr.e_phnum\n");
    return -1;
  }

  // Elf places the PT_LOAD segments in ascending order of vaddresses.
  // So we find the last one to calculate the whole address span of the image.
  auto *first_load = &phdr[i];
  auto *last_load = &phdr[ehdr.e_phnum - 1];

  // walk the last_load back
  while (last_load > first_load && last_load->p_type != PT_LOAD) last_load--;

  // Total memory size of phdr between first and last PT_LOAD.
  // size_t span = last_load->p_vaddr + last_load->p_memsz -
  // first_load->p_vaddr;

  for (int i = 0; i < ehdr.e_phnum; i++) {
    auto &sec = phdr[i];

    // printk("fsz=%zu, msz=%zu\n", sec.p_filesz, sec.p_memsz);

    if (sec.p_type == PT_LOAD) {
      auto start = round_down(sec.p_vaddr, 1);

      if (sec.p_filesz < sec.p_memsz) {
        // printk("    is .bss\n");
      }

      auto prot = 0L;
      if (sec.p_flags & PF_X) prot |= PROT_EXEC;
      if (sec.p_flags & PF_W) prot |= PROT_WRITE;
      if (sec.p_flags & PF_R) prot |= PROT_READ;


			auto flags = 0L;
			if (prot & PROT_WRITE) {
				flags = MAP_PRIVATE;
			} else {
				flags = MAP_SHARED;
			}

			// printk("%p (%zu), prot: %x, flags: %x\n", off + start, sec.p_memsz, prot, flags);

			// printk("mmap[%d] addr=%p, size=%08x, offset=%08x\n", i, off + start, sec.p_memsz, sec.p_offset);
      mm.mmap(path, off + start, sec.p_memsz, prot, flags, fd, sec.p_offset);
      handle_bss(sec);
    }
  }

  delete[] phdr;

  return 0;
}
