#pragma once
#ifndef __ARCH_H__
#define __ARCH_H__

// Architecture specific functionality. Implemented in arch/$ARCH/*
namespace arch {
void cli(void);
void sti(void);

void halt(void);

// invalidate a page mapping
void invalidate_page(unsigned long addr);

void flush_tlb(void);

unsigned long read_timestamp(void);
};  // namespace arch

#endif
