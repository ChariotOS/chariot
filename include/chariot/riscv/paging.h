#pragma once

#include <riscv/arch.h>
#include <mm.h>
#include <riscv/paging_flags.h>

namespace rv {

  using pte_t = rv::xsize_t;


  class pagetable : public mm::pagetable {
   public:
    rv::xsize_t *table;
    pagetable(rv::xsize_t *table);
    virtual ~pagetable();

    virtual bool switch_to(void) override;

    virtual int add_mapping(off_t va, struct mm::pte &) override;
    virtual int get_mapping(off_t va, struct mm::pte &) override;
    virtual int del_mapping(off_t va) override;
  };


  rv::pte_t *page_walk(rv::pte_t *tbl, off_t va);
};  // namespace rv
