#pragma once


#include <ck/utility.h>
#include <ck/func.h>
#include <ck/option.h>
#include <ck/lock.h>
#include <ck/fiber.h>

namespace ck {


  template <typename R>
  struct future_control : public ck::refcounted<future_control<R>> {
    ck::func<void(R)> on_ready;
    ck::option<R> value;
    bool fired = false;
    void fire(void) {
      assert(!fired);
      assert(on_ready);
      assert(value.has_value());

      on_ready(value.take());
      fired = true;
    }

    void resolve(R&& v) {
      auto l = lock();
      assert(!value.has_value());
      value = move(v);
      if (on_ready) {
        fire();
      }
    }

    ck::scoped_lock lock(void) { return ck::scoped_lock(m_lock); }

   private:
    ck::mutex m_lock;
  };



  template <typename F, typename R>
  concept future_callback = requires(F f, R r) {
    f(r);
  };


  template <typename R>
  class future : public ck::refcounted<future<R>> {
   public:
    typedef R value_type;
    using ref = ck::ref<future<R>>;


    future(R&& v) noexcept { resolve(move(v)); }
    future() noexcept {}
    future(future<R>& other) : control(other.get_control()) {}                           // copy
    future(future<R>&& other) : control(other.get_control()) { other.control.clear(); }  // move


    future<R>& operator=(future<R>& other) {
      control = other.get_control();
      return *this;
    }
    future<R>& operator=(future<R>&& other) {
      control = other.get_control();
      // other.control = nullptr;
      return *this;
    }

    virtual ~future() { control.clear(); }

    // upon resolution, call this callback. A future can only have one callback at a time
    void then(ck::func<void(R)> f) {
      // assert that there is not already an associated continuation
      assert(!get_control()->on_ready);
      // set the continuation
      get_control()->on_ready = move(f);
      // if the future is already ready, call the continuation immediately
      if (get_control()->value.has_value()) {
        get_control()->fire();
      }
    }

    // upon resolution, map the value to a new value and return that value as a future
    template <future_callback<R> F>
    auto map(F f) -> ck::future<decltype(f(R{}))> {
      using T = decltype(f(R{}));
      // the future that gets resolved by the return value of f
      ck::future<T> c;
      // get a weak reference to the control structure of the new future
      auto c_ctrl = c.get_control();
      this->then([c_ctrl, this, f](R r) {
        auto ctrl = c_ctrl.get();
        if (ctrl) {
          ctrl->resolve(move(f(move(r))));
        }
      });

      return move(c);
    }

    void resolve(R&& v) { get_control()->resolve(move(v)); }

    // A future has been resolved if it has a value or it has fired.
    bool resolved(void) {
      auto ctrl = get_control();
      auto l = ctrl->lock();
      return ctrl->value.has_value() || fired();
    }

    bool fired(void) {
      auto ctrl = get_control();
      auto l = ctrl->lock();
      return ctrl->fired;
    }

    void clear(void) { this->control.clear(); }


    R await(void) {
      ck::fiber& f = ck::fiber::current();
      R return_value{0};
      f.set_state(ck::FiberState::Paused);

      this->then([&](auto val) {
        return_value = val;
        f.set_ready();
      });

      f.yield(f.state());

      return return_value;
    }
    // get the control as a refcount, as if you are calling this method, the future is still alive.
    ck::ref<future_control<R>> get_control(void) { return control = control ?: ck::make_ref<future_control<R>>(); }

    //  protected:
    ck::ref<future_control<R>> control;
  };



  // special case implementation for void
  template <>
  struct future_control<void> : public ck::refcounted<future_control<void>> {
    ck::func<void(void)> on_ready;
    bool fired = false;
    bool ready = false;
    void fire(void) {
      assert(!fired);
      assert(on_ready);
      on_ready();
      fired = true;
      ready = false;
    }
    void resolve() {
      auto l = lock();
      ready = true;
      if (on_ready) {
        fire();
      }
    }
    ck::scoped_lock lock(void) { return ck::scoped_lock(m_lock); }

   private:
    ck::mutex m_lock;
  };

  template <>
  class future<void> : public ck::refcounted<future<void>> {
   public:
    using ref = ck::ref<future<void>>;


    future() noexcept {}
    future(future<void>& other) : control(other.get_control()) {}                           // copy
    future(future<void>&& other) : control(other.get_control()) { other.control.clear(); }  // move
    future<void>& operator=(future<void>& other) {
      control = other.get_control();
      return *this;
    }
    future<void>& operator=(future<void>&& other) {
      control = other.get_control();
      // other.control = nullptr;
      return *this;
    }

    virtual ~future() { control.clear(); }

    // upon resolution, call this callback. A future can only have one callback at a time
    void then(ck::func<void(void)> f) {
      // assert that there is not already an associated continuation
      assert(!get_control()->on_ready);

      auto l = get_control()->lock();
      // set the continuation
      get_control()->on_ready = move(f);
      // if the future is already ready, call the continuation immediately
      if (get_control()->ready) {
        get_control()->fire();
      }
    }

    // upon resolution, map the value to a new value and return that value as a future
    template <typename F>
    auto map(F f) -> ck::future<decltype(f())> {
      using T = decltype(f());
      // the future that gets resolved by the return value of f
      ck::future<T> c;
      // get a weak reference to the control structure of the new future
      auto c_ctrl = c.get_control();
      this->then([c_ctrl, this, f]() {
        auto ctrl = c_ctrl.get();
        if (ctrl) {
          ctrl->resolve(move(f()));
        }
      });

      return move(c);
    }

    void resolve(void) { get_control()->resolve(); }

    // A future has been resolved if it has a value or it has fired.
    bool resolved(void) {
      auto ctrl = get_control();
      auto l = ctrl->lock();
      return ctrl->ready || fired();
    }

    bool fired(void) {
      auto ctrl = get_control();
      auto l = ctrl->lock();
      return ctrl->fired;
    }

    void clear(void) { this->control.clear(); }


    void await(void) {
      ck::fiber& f = ck::fiber::current();
      f.set_state(ck::FiberState::Paused);

      this->then([&]() {
        f.set_ready();
      });

      f.yield(f.state());
    }
    // get the control as a refcount, as if you are calling this method, the future is still alive.
    ck::ref<future_control<void>> get_control(void) { return control = control ?: ck::make_ref<future_control<void>>(); }

    //  protected:
    ck::ref<future_control<void>> control;
  };


  // template <typename R>
  // ck::ref<ck::future<R>> make_future(void) {
  //   return ck::make_ref<ck::future<R>>();
  // }

}  // namespace ck

#define async(T) __attribute__((noinline)) ck::future<T>
