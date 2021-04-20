#ifndef __X86_H
#define __X86_H

#include <ck/func.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ck/io.h>

namespace x86 {

  class code;
  struct inst;
  enum reg {
    eax = 0,
    ecx = 1,
    edx = 2,
    ebx = 3,
    esp = 4,
    ebp = 5,
    esi = 6,
    edi = 7,
  };

  struct inst {
    // REX allows an instruction to be expanded
    // into 64bit.
    // gives ability to have 64-vit virtual addresses and instruction pointers.
    // When has_rex, gprs are widened to 64-bit
    bool has_rex = false;
    // REX.W -
    // REX.R Extends modrm reg
    // REX.X Extends the index of SIB
    // REX.B Extends the Base of modrm
    uint8_t rex = 0x40;
    inline void set_rex(bool to) {
      has_rex = to;
      rex |= 0x40;
    }

    inline void change_mask_rex_params(uint8_t mask, bool on) {
      if (on) {
        rex |= mask;
      } else {
        rex &= ~mask;
      }
    }
    inline void set_rexw(bool to) {
      has_rex = true;
      change_mask_rex_params(0b1000, to);
    }
    inline void set_rexr(bool to) {
      has_rex = true;
      change_mask_rex_params(0b0100, to);
    }
    inline void set_rexx(bool to) {
      has_rex = true;
      change_mask_rex_params(0b0010, to);
    }
    inline void set_rexb(bool to) {
      has_rex = true;
      change_mask_rex_params(0b0001, to);
    }

    // an x86 instruction can have 1, 2, or 3 bytes of opcode
    uint8_t opcode_size = 1;
    uint8_t opcode[3];

    inline void set_opcode(uint8_t o1) {
      opcode_size = 1;
      opcode[0] = o1;
    }
    inline void set_opcode(uint8_t o1, uint8_t o2) {
      opcode_size = 2;
      opcode[0] = o1;
      opcode[1] = o2;
    }
    inline void set_opcode(uint8_t o1, uint8_t o2, uint8_t o3) {
      opcode_size = 3;
      opcode[0] = o1;
      opcode[1] = o2;
      opcode[1] = o3;
    }

    // modRM: mode-register-memory
    // mod: 4 addressing modes
    //        -  11b: register-direct
    //        - !11b: register-indirect modes, disp. specialization follows
    // reg: register based operand or extend operation encoding
    // rm: register or memory operand when combined with mod field.
    //     addressing mode can include a follwing SIB byte (mod=00b,rm=101b)
    bool has_modrm = false;
    struct {
      uint8_t mod;
      uint8_t reg;
      uint8_t rm;
    };

    inline void set_modrm(bool direct, int reg, int rm) {
      has_modrm = true;
      mod = direct ? 0b11 : 0b00;
      this->reg = reg;
      this->rm = rm;
    }

    // SIB: scale-index-base
    // scale: scale factor
    // index: reg containing the index portion
    // base: reg containing the base portion
    // eff_addr = scale * index + base + offset
    // TODO: where does offset come from?
    bool has_sib = false;
    struct {
      uint8_t scale;
      uint8_t index;
      uint8_t base;
    };

    inline void set_sib(int scale, int index, int base) {
      // TODO
      has_sib = true;
      this->scale = scale;
      this->index = index;
      this->base = base;
    }

    // displacement
    // signed offset.
    // - absolute: added to the base of the code segment
    // - relative: relative to EIP
    // 1, 2, 4, or 8 bytes (8 only if operator is extended)
    // sign-extended in 64-bit mode if operand 64-bit
    uint8_t disp_size = 0;
    uint8_t displacement[8];

    // immediates
    // encoded in the instruction, come last
    // 1, 2, 4, or 8 bytes
    // with def. operand size in 64-bit mode, sign extended.
    // MOV-to-GPR (A0-A3) vfersions can specify 64-bit immediate
    // absolute address called moffset
    uint8_t imm_size = 0;
    uint8_t immediate[8];

    template <typename T>
    inline void set_immediate(T val) {
      imm_size = sizeof(T);
      if (imm_size > 8) {
        panic("immediate instruction size may not be more than 8 bytes");
      }
      uint8_t *ptr = (uint8_t *)(void *)&val;
      for (int i = 0; i < imm_size; i++) {
        immediate[i] = ptr[i];
      }
    }

    template <typename T>
    inline void set_displacement(T val) {
      disp_size = sizeof(T);
      if (imm_size > 8) {
        panic("immediate instruction size may not be more than 8 bytes");
      }
      uint8_t *ptr = (uint8_t *)(void *)&val;
      for (int i = 0; i < disp_size; i++) {
        displacement[i] = ptr[i];
      }
    }

    void encode(x86::code &code);

    // the size, in bytes, of this instruction
    uint32_t size();

    inline inst(void) {
      clear();
    }

    inline void clear() {
      set_opcode(0x90);
      has_rex = false;
      has_modrm = false;
      has_sib = false;
      disp_size = 0;
      imm_size = 0;
    }
  };

  inline inst push(x86::reg r1) {
    inst i;
    i.set_rex(false);
    i.set_opcode(0x50 + r1);
    return i;
  }
  inline inst pop(x86::reg r1) {
    inst i;
    i.set_rex(false);
    i.set_opcode(0x58 + r1);
    return i;
  }

  inline inst ret() {
    inst i;
    i.set_opcode(0xC3);
    return i;
  }

  inline inst add(x86::reg r1, x86::reg r2) {
    inst i;
    i.set_rexw(true);
    i.set_opcode(0x01);
    i.set_modrm(true, r2, r1);
    return i;
  }

  inline inst add(x86::reg dest, int32_t num) {
    inst i;

    // special case for adding to eax
    if (dest == x86::eax) {
      i.set_opcode(0x05);
      i.set_immediate<int32_t>(num);
    } else {
      i.set_opcode(0x81);
      i.set_modrm(true, 0, dest);
      i.set_immediate<int32_t>(num);
    }
    // 32bit. no rex
    return i;
  }

  // TODO: implement MOV
  inline inst mov(x86::reg r1, x86::reg r2) {
    inst i;
    i.set_rexw(true);
    i.set_opcode(0x89);
    i.set_modrm(true, r2, r1);
    return i;
  }

  class code {
   protected:
    // size is how many bytes are written into the code pointer
    // it also determines *where* to write when writing new data
    uint64_t size = 0;
    // cap is the number of bytes allocated for the code pointer
    uint64_t cap = 32;

    // the code pointer is where this instance's bytecode is actually
    // stored. All calls for read interpret a type from this byte vector
    // and calls for write append to it.
    uint8_t *buf;

    bool finalized = false;

    void *mapped_region = nullptr;

   public:
    inline code() {
      cap = 32;
      buf = new uint8_t[cap];
    }

    inline ~code() {
      if (mapped_region != nullptr && finalized) {
        munmap(mapped_region, size);
      }
    }

    inline x86::code &operator<<(inst i) {
      auto *c = buf + size;
      i.encode(*this);

      ck::hexdump(c, i.size());

      return *this;
    }

    template <typename T>
    inline T read(uint64_t i) const {
      return *(T *)(void *)(buf + i);
    }

    template <typename T>
    inline uint64_t write_to(uint64_t i, T val) const {
      if (finalized) panic("x86: unable to modify finalized code");
      *(T *)(void *)(buf + i) = val;
      return size;
    }

    template <typename T>
    inline uint64_t write(T val) {
      if (finalized) panic("x86: unable to modify finalized code");
      if (size + sizeof(val) >= cap - 1) {
        uint8_t *new_code = new uint8_t[cap * 2];
        memcpy(new_code, buf, cap);
        cap *= 2;
        delete buf;
        buf = new_code;
      }
      uint64_t addr = write_to(size, val);
      size += sizeof(T);
      return addr;
    }

    inline void bytes(void) {
    }
    inline void bytes(unsigned char c) {
      write<unsigned char>(c);
    }

    template <typename... Args>
    inline void bytes(unsigned char c, Args const &...args) {
      bytes(c);
      bytes(args...);
    }

    inline uint64_t get_size() {
      return size;
    }
    inline uint64_t get_cap() {
      return cap;
    }

    inline void copy(void *dest) {
      memcpy(dest, buf, size);
    }

    static inline int dump_bytes(uint8_t *code, size_t len) {
      return 1;
    };

    template <typename R, typename... Args>
    ck::func<R(Args...)> finalize(void) {
      if (finalized) panic("x86: unable re-finalize code");

      finalized = true;
      ck::func<R(Args...)> f;

      auto ptr = (R(*)(Args...))mmap(NULL, get_size(), PROT_READ | PROT_EXEC | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE, -1, 0);

      mapped_region = (void *)ptr;
      copy((char *)ptr);
      f = ptr;
      return f;
    }

    inline int dump(void) {
      return dump_bytes(buf, size);
    }

    void prologue(void);
    void epilogue(void);
  };

};  // namespace x86
#endif
