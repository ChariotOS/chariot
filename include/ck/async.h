#include <setjmp.h>
#include <unistd.h>

namespace ck {



  /* TODO: :^) */
  class future {};


  /* The base runtime structure */
  class runtime {};


  /* A stackful coroutine that can be switched between */
  class fiber {
    void *stack;
    size_t stack_size;
		/* TODO: an implementation */
  };

};  // namespace ck
