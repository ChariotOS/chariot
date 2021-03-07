#pragma once

#include <types.h>

namespace rv {

  /* Simple risc-v disassembler */
  class disassembler {
   private:
    uint32_t insn;

    char m_op0[32];
    char m_op1[32];
    char m_op2[32];


   public:
    void dis(uint32_t insn);
    void dump(uint32_t insn);
  };
};  // namespace rv
