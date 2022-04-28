#pragma once

#include <riscv/arch.h>
#include <mm.h>
#include <riscv/paging_flags.h>

namespace rv {

  using pte_t = rv::xsize_t;


  class PageTable final : public mm::TransactionBasedPageTable {
   public:
    rv::xsize_t *table;
    PageTable(rv::xsize_t *table);
    virtual ~PageTable();

    bool switch_to(void) override;

    int get_mapping(off_t va, struct mm::pte &) override;
    void commit_mappings(ck::vec<mm::PendingMapping> &mappings) override;

		uint64_t hart_mask = 0;
		uint64_t remote_invalidations = 0;

  };


  rv::pte_t *page_walk(rv::pte_t *tbl, off_t va);
};  // namespace rv
