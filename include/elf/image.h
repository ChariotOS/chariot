#pragma once

#include <types.h>
#include "exec_elf.h"

namespace elf {
class image {
  u8* data;
  int len;

  bool m_valid{false};
  unsigned m_symbol_table_section_index {0};
  unsigned m_string_table_section_index {0};

 public:
  class section;

  class symbol {
   public:
    symbol(const image& image, unsigned index, const Elf64_Sym& sym)
        : m_image(image), m_sym(sym), m_index(index) {}

    ~symbol() {}

    const char* name() const { return m_image.table_string(m_sym.st_name); }
    unsigned section_index() const { return m_sym.st_shndx; }
    unsigned value() const { return m_sym.st_value; }
    unsigned size() const { return m_sym.st_size; }
    unsigned index() const { return m_index; }
    unsigned type() const { return ELF32_ST_TYPE(m_sym.st_info); }
    const section get_section() const {
      return m_image.get_section(section_index());
    }

   private:
    const image& m_image;
    const Elf64_Sym& m_sym;
    const unsigned m_index;
  };
  class section {
   public:
    section(const image& image, unsigned sectionIndex)
        : m_image(image),
          m_section_header(image.section_header(sectionIndex)),
          m_section_index(sectionIndex) {}
    ~section() {}

    const char* name() const {
      return m_image.section_header_table_string(m_section_header.sh_name);
    }
    unsigned type() const { return m_section_header.sh_type; }
    unsigned offset() const { return m_section_header.sh_offset; }
    unsigned size() const { return m_section_header.sh_size; }
    unsigned entry_size() const { return m_section_header.sh_entsize; }
    unsigned entry_count() const {
      return !entry_size() ? 0 : size() / entry_size();
    }
    u64 address() const { return m_section_header.sh_addr; }
    const u8* raw_data() const {
      return m_image.raw_data(m_section_header.sh_offset);
    }
    bool is_undefined() const { return m_section_index == SHN_UNDEF; }
    u32 flags() const { return m_section_header.sh_flags; }
    bool is_writable() const { return flags() & SHF_WRITE; }
    bool is_executable() const { return flags() & PF_X; }

   private:
    //         friend class RelocationSection;
    const image& m_image;
    const Elf64_Shdr& m_section_header;
    unsigned m_section_index;
  };

  image(u8* data, int len);

  inline bool valid(void) { return m_valid; }

  const u8* raw_data(int off = 0) const;

  const Elf64_Ehdr& header() const;
  const Elf64_Shdr& section_header(unsigned index) const;
  unsigned section_count() const;
  unsigned symbol_count() const;

  const section get_section(unsigned index) const;

  void dump();

  const char* table_string(unsigned offset) const;
  const char* section_header_table_string(unsigned offset) const;

  const symbol get_symbol(unsigned index) const;


const char* section_index_to_string(unsigned index) const;

};
};  // namespace elf
