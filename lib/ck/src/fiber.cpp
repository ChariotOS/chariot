#include <ck/fiber.h>
#include <alloca.h>

static void* ck_fiber_setsp_unoptimisable;

#define set_stack_pointer(x) \
  ck_fiber_setsp_unoptimisable = alloca((char*)alloca(sizeof(size_t)) - (char*)(x));

#pragma GCC push_options
#pragma GCC optimize("O0")



// TODO: threadlocal!
static ck::fiber* s_current_fiber;

extern "C" void _call_with_new_stack(void* sp, void* func, void* arg);


ck::fiber& ck::fiber::current(void) {
  assert(s_current_fiber);
  return *s_current_fiber;
}


void ck::fiber::new_stack_callback(void* _c) {
  fiber* c = (fiber*)_c;
  printf("c: %p\n", c);
  c->func(c);
  c->m_done = true;
  // switch back to the calling context
  longjmp(c->m_return_ctx, 1);
}

void ck::fiber::yield(ck::FiberState s) {
  if (setjmp(ctx) == 0) {
    m_state = s;
    s_current_fiber = NULL;
    longjmp(m_return_ctx, 1);
  }
}

#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))



void ck::fiber::resume(void) {
  s_current_fiber = this;
  assert(m_state == ck::FiberState::Ready);

  if (is_done()) {
    fprintf(stderr, "Error: Resuming a complete fiber\n");
    return;
  }

  if (setjmp(m_return_ctx) == 0) {
    if (!m_initialized) {
      m_initialized = true;
      // calculate the new stack pointer
      size_t sp = (size_t)(stk);
      sp += STACK_SIZE;
      sp = ALIGN(sp, 16);
      _call_with_new_stack((void*)((char*)sp - 16), (void*)new_stack_callback, (void*)this);
    } else {
      longjmp(ctx, 1);
    }
  }
  if (is_done() && on_exit) on_exit(this);
  return;
}


void ck::fiber::set_ready(void) { m_state = FiberState::Ready; }

#pragma GCC pop_options
