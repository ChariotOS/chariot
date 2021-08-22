#include <ck/fiber.h>
#include <alloca.h>

static void* ck_fiber_setsp_unoptimisable;

#define set_stack_pointer(x) \
  ck_fiber_setsp_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));



extern "C" void _call_with_new_stack(void* sp, void* func, void* arg);

void ck::fiber::new_stack_callback(void* _c) {
  fiber* c = (fiber*)_c;
  c->func(c);
  c->m_done = true;
  // switch back to the calling context
  longjmp(c->m_return_ctx, 1);
}

void ck::fiber::yield(void) {
  if (setjmp(ctx) == 0) {
    longjmp(m_return_ctx, 1);
  }
}

void ck::fiber::resume(void) {
  if (is_done()) {
    fprintf(stderr, "Error: Resuming a complete fiberutine\n");
    return;
  }

  if (setjmp(m_return_ctx) == 0) {
    if (!m_initialized) {
      m_initialized = true;
      // calculate the new stack pointer
      size_t sp = (size_t)(stk);
      sp += STACK_SIZE;
      sp &= ~15;

      // set_stack_pointer(sp - 8);
      // func(this);

      _call_with_new_stack((void*)((char*)sp - 8), (void*)new_stack_callback, (void*)this);
      return;
    }
    longjmp(ctx, 1);
    return;
  }

  return;
}