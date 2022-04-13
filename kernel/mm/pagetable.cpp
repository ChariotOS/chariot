#include <mm.h>
#include <thread.h>

mm::PageTable::PageTable() {
  // TODO: add to some global list
}


mm::PageTable::~PageTable() {
  // TODO: remove from some global list
}


void *mm::PageTable::translate(off_t va) {
  struct pte pte;

  off_t off = va & 0xFFF;
  off_t vpage = va & ~0xFFF;

  int res = get_mapping(vpage, pte);

  // printf("translate(%p) -> %d\n", vpage, res);

  off_t ppage = pte.ppn;
  if (ppage == 0) return NULL;

  return (void *)((ppage << 12) + off);
}


int mm::TransactionBasedPageTable::add_mapping(off_t va, struct mm::pte &pte) {
  assert(in_transaction);
  pending_mappings.push({.cmd = mm::PendingMapping::Command::Map, .va = va, .pte = pte});
  return 0;
}


int mm::TransactionBasedPageTable::del_mapping(off_t va) {
  assert(in_transaction);
  pending_mappings.push({.cmd = mm::PendingMapping::Command::Delete, .va = va});
  return 0;
}


void mm::TransactionBasedPageTable::transaction_begin(const char *reason) {
  lock.lock();
  // tx_reason = reason;
  in_transaction = true;
}

void mm::TransactionBasedPageTable::transaction_commit() {
  commit_mappings(pending_mappings);
  pending_mappings.clear();

  in_transaction = false;
  lock.unlock();
}
