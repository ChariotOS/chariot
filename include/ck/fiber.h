#pragma once

#include <ck/ptr.h>
#include <ck/object.h>
#include <ck/func.h>
#include <setjmp.h>

namespace ck {


  enum class FiberState {
    Ready,
    Paused,
  };


  class fiber : public ck::weakable<fiber> {
    CK_NONCOPYABLE(fiber);
    CK_MAKE_NONMOVABLE(fiber);

   public:
    using Fn = ck::func<void(fiber*)>;

    fiber(Fn fn) : func(move(fn)) {}
    ~fiber(void) {
      if (stk != nullptr) {
        free(stk);
      }
    }

    void yield(FiberState s = FiberState::Paused);
    void yield_ready(void) { yield(FiberState::Ready); }
    bool is_done(void) const { return m_done; }
    void resume(void);

    void set_ready(void);
    FiberState state(void) const { return m_state; }

    void set_state(FiberState s) { m_state = s; }

    // mainly used for join() like functionality in the eventloop
    ck::func<void(ck::fiber*)> on_exit;

    static ck::fiber& current(void);
    static bool in_fiber(void);

   private:
    jmp_buf ctx;
    jmp_buf m_return_ctx;
    bool m_done = false;

    FiberState m_state = FiberState::Ready;
    Fn func;

    // static ck::weak_ref<fiber> spawn(Fn) {}

    char* stk = nullptr;
    bool m_initialized = false;

    static constexpr long STACK_SIZE = 4096 * 4;  // 16kb of stack, which should be enough.

    static void new_stack_callback(void* _c);
  };



}  // namespace ck
