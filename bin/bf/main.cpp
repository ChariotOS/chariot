#include <ck/defer.h>
#include <ck/io.h>
#include <ck/string.h>
#include <ck/vec.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "x86.h"

class interpreter {
  off_t pc = 0;
  off_t ptr = 0;
  unsigned char *code;
  size_t codesize;
  unsigned char *mem;
  size_t memorysize;

  // enough
  int loopskip = 0;
  int loopdepth = 0;
  int loop[255];

 public:
  interpreter(unsigned char *code, size_t codesize, size_t memorysize = 65535) {
    this->code = code;
    this->codesize = codesize;
    this->memorysize = memorysize;
    mem = (unsigned char *)malloc(memorysize);
  }

  ~interpreter() {
    free(mem);
  }


  int execute(void) {
    while (1) {
      if (pc > (off_t)codesize) {
        // if we run beyond the end of code, we consider it to be done
        break;
      }

      auto insn = code[pc++];

      switch (insn) {
        case '>':
          if (!loopskip) ptr++;
          break;
        case '<':
          if (!loopskip) ptr--;
          break;
        case '+':
          if (!loopskip) {
            mem[ptr]++;
          }
          break;
        case '-':
          if (!loopskip) mem[ptr]--;
          break;
        case '.':
          if (!loopskip) {
            printf("%c", mem[ptr]);
            fflush(stdout);
          }
          break;
        case ',':
          if (!loopskip) {
            mem[ptr] = getchar();
          }
          break;
        case '[':
          if (!loopskip) {
            if ((size_t)loopdepth >= sizeof(loop) - 1) {
              fprintf(stderr, "bf: too deep loops\n");
              exit(EXIT_FAILURE);
            }
          }
          loop[loopdepth++] = pc;
          if (!loopskip) {
            if (mem[ptr] == 0) {
              loopskip = loopdepth;
            }
          }
          break;

        case ']':

          if (--loopdepth < 0) {
            fprintf(stderr, "bf: Invalid syntax at index %lld\n", pc - 1);
            exit(EXIT_FAILURE);
          }
          if (!loopskip) {
            if (mem[ptr] != 0) {
              pc = loop[loopdepth] - 1;
            }
          } else {
            if (mem[ptr] == 0) {
              if (loopskip == loopdepth + 1) {
                loopskip = 0;
              }
            }
          }
          break;
        default:
          break;
      }
    }

    return 0;
  }
};


int main(int argc, const char **argv) {
  if (0) {
    // create a code object
    x86::code code;
    // fill out the instructions
    code.prologue();  // prologue of the function, setup stack frame
    code << x86::add(x86::edi, x86::esi);
    code << x86::mov(x86::eax, x86::edi);
    code.epilogue();  // pop rbp and return
    // print the x86 mnemonics (using capstone)
    code.dump();
    // finalize the function and call it!
    auto func = code.finalize<long, long, long>();
    long res = func(1, 2);
    assert(res == 3);
    printf("1 + 2 = %ld\n", res);
  }


  const char *file = "/usr/src/bf/hello.bf";

  if (argc == 2) {
    file = argv[1];
  }

  // open the file!
  auto stream = ck::file(file, "r");

  if (!stream) {
    fprintf(stderr, "Failed to open '%s'\n", file);
    exit(EXIT_FAILURE);
  }

  // map the file into memory and parse it for files
  size_t size = stream.size();
  auto code = (unsigned char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, stream.fileno(), 0);

  interpreter i(code, size);

  i.execute();

  munmap(code, size);

  return 0;
}
