#pragma once

#include <ck/ptr.h>
#include <ck/object.h>
#include <ck/func.h>
#include <setjmp.h>
#include <ck/future.h>

namespace ck {


  enum FiberState {
    FIBER_READY,
    FIBER_PAUSED,
  };


  class fiber : public ck::weakable<fiber> {
    CK_NONCOPYABLE(fiber);
    CK_MAKE_NONMOVABLE(fiber);

   public:
    using Fn = ck::func<void(fiber*)>;


    fiber(Fn fn) : func(move(fn)) {
      stk = (char*)malloc(STACK_SIZE);
      stack_base = stk;
      stack_base += STACK_SIZE;
    }
    ~fiber(void) { free(stk); }

    void yield(void);
    bool is_done(void) const { return m_done; }
    void resume(void);

   private:
    jmp_buf ctx;
    jmp_buf m_return_ctx;
    bool m_done = false;

    FiberState m_state = FIBER_READY;
    Fn func;

    // static ck::weak_ref<fiber> spawn(Fn) {}

    char* stk = nullptr;
    char* stack_base = nullptr;
    bool m_initialized = false;

    static constexpr long STACK_SIZE = 4096 * 8;

    static void new_stack_callback(void* _c);
  };

}  // namespace ck