#include <elf/image.h>
#include <elf/loader.h>
#include <printk.h>
#include <util.h>

static const char *object_file_type_to_string(Elf32_Half type) {
  switch (type) {
    case ET_NONE:
      return "None";
    case ET_REL:
      return "Relocatable";
    case ET_EXEC:
      return "Executable";
    case ET_DYN:
      return "Shared object";
    case ET_CORE:
      return "Core";
    default:
      return "(?)";
  }
}

elf::image::image(u8 *data, int len) : data(data), len(len) {
  static const char elf_header[] = {0x7f, 0x45, 0x4c, 0x46};

  // checking if its an elf binary?
  if (len < 4) return;
  for (int i = 0; i < 4; i++) {
    if (data[i] != elf_header[i]) {
      m_valid = false;
      return;
    }
  }
  /*
  if (header().e_machine != 3) {
    printk("ELFImage::parse(): e_machine=%u not supported!\n");
    m_valid = false;
    return;
  }
  */

  m_symbol_table_section_index = 0;
  m_string_table_section_index = 0;

  // First locate the string tables.
  for (unsigned i = 0; i < section_count(); ++i) {
    auto &sh = section_header(i);
    if (sh.sh_type == SHT_SYMTAB) {
      assert(!m_symbol_table_section_index);
      m_symbol_table_section_index = i;
    }
    if (sh.sh_type == SHT_STRTAB && i != header().e_shstrndx) {
      // assert(m_string_table_section_index == 0);
      m_string_table_section_index = i;
    }
  }
  m_valid = true;
}

const u8 *elf::image::raw_data(int off) const { return data + off; }

const Elf64_Ehdr &elf::image::header() const {
  return *reinterpret_cast<const Elf64_Ehdr *>(raw_data(0));
}

void elf::image::dump() {
  printk("Elf Image (%p): \n", this);

  printk("    type:    %s\n", object_file_type_to_string(header().e_type));
  printk("    machine: %u\n", header().e_machine);
  printk("    entry:   %x\n", header().e_entry);
  printk("    shoff:   %u\n", header().e_shoff);
  printk("    shnum:   %u\n", header().e_shnum);
  printk(" shstrndx:   %u\n", header().e_shstrndx);

  for (unsigned i = 0; i < header().e_shnum; ++i) {
    auto &section = this->get_section(i);
    printk("    Section %u: {\n", i);
    printk("        name: %s\n", section.name());
    printk("        type: %x\n", section.type());
    printk("      offset: %x\n", section.offset());
    printk("       vaddr: %p\n", section.address());
    printk("        size: %u\n", section.size());
    hexdump((void *)section.raw_data(), section.size());
    printk("        \n");
    printk("    }\n");
  }

  printk("Symbol count: %u (table is %u)\n", symbol_count(),
         m_symbol_table_section_index);
  for (unsigned i = 1; i < symbol_count(); ++i) {
    auto &sym = get_symbol(i);
    printk("Symbol @%u:\n", i);
    printk("    Name: %s\n", sym.name());
    printk("    In section: %s\n",
           section_index_to_string(sym.section_index()));
    printk("    Value: %x\n", sym.value());
    printk("    Size: %u\n", sym.size());
  }
}

unsigned elf::image::symbol_count() const {
  return get_section(m_symbol_table_section_index).entry_count();
}

const Elf64_Shdr &elf::image::section_header(unsigned index) const {
  return *reinterpret_cast<const Elf64_Shdr *>(
      raw_data(header().e_shoff + (index * sizeof(Elf64_Shdr))));
}

const elf::image::section elf::image::get_section(unsigned index) const {
  return section(*this, index);
}

const elf::image::symbol elf::image::get_symbol(unsigned index) const {
  assert(index < symbol_count());
  auto *raw_syms = reinterpret_cast<const Elf64_Sym *>(
      raw_data(get_section(m_symbol_table_section_index).offset()));
  return symbol(*this, index, raw_syms[index]);
}

const char *elf::image::section_header_table_string(unsigned offset) const {
  auto &sh = section_header(header().e_shstrndx);
  if (sh.sh_type != SHT_STRTAB) return nullptr;
  return (const char *)raw_data(sh.sh_offset + offset);
}

const char *elf::image::table_string(unsigned offset) const {
  auto &sh = section_header(m_string_table_section_index);
  if (sh.sh_type != SHT_STRTAB) return nullptr;
  return (const char *)raw_data(sh.sh_offset + offset);
}

unsigned elf::image::section_count() const

{
  return header().e_shnum;
}

const char *elf::image::section_index_to_string(unsigned index) const {
  if (index == SHN_UNDEF) return "Undefined";
  if (index >= SHN_LORESERVE) return "Reserved";
  return get_section(index).name();
}

