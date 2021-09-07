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


    future() noexcept;
    future(future<R>& other) : control(other.get_control()) {}   // copy
    future(future<R>&& other) : control(other.get_control()) {}  // move
    future<R>& operator=(future<R>& other) {
      control = other.get_control();
      return *this;
    }
    future<R>& operator=(future<R>&& other) {
      control = other.get_control();
      // other.control = nullptr;
      return *this;
    }

    virtual ~future();

    // upon resolution, call this callback. A future can only have one callback at a time
    void then(ck::func<void(R)> f);
    // upon resolution, map the value to a new value and return that value as a future
    template <future_callback<R> F>
    auto map(F f) -> ck::future<decltype(f(R{}))>;
    void resolve(R&& v) { get_control()->resolve(move(v)); }
    // A future has been resolved if it has a value or it has fired.
    bool resolved(void) const;
    bool fired(void) const;
    void clear(void);


    R await(void);
    // get the control as a refcount, as if you are calling this method, the future is still alive.
    ck::ref<future_control<R>> get_control(void);

    //  protected:
    ck::ref<future_control<R>> control;
  };


  template <typename R>
  ck::ref<ck::future<R>> make_future(void) {
    return ck::make_ref<ck::future<R>>();
  }

}  // namespace ck

#define async(T) __attribute__((noinline)) ck::future<T>

/// Implementation for the future

template <typename R>
R ck::future<R>::await(void) {
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

template <typename R>
ck::future<R>::future() noexcept {}

template <typename R>
ck::future<R>::~future() {
  control.clear();
}

template <typename R>
bool ck::future<R>::resolved() const {
  auto ctrl = get_control();
  auto l = ctrl.lock();

  return ctrl->value.has_value() || fired();
}

template <typename R>
bool ck::future<R>::fired() const {
  auto ctrl = get_control();
  auto l = ctrl.lock();

  return ctrl->fired;
}

template <typename R>
ck::ref<ck::future_control<R>> ck::future<R>::get_control() {
  return control = control ?: ck::make_ref<future_control<R>>();
}


template <typename R>
void ck::future<R>::then(ck::func<void(R)> f) {
  // assert that there is not already an associated continuation
  assert(!get_control()->on_ready);
  // set the continuation
  get_control()->on_ready = move(f);
  // if the future is already ready, call the continuation immediately
  if (get_control()->value.has_value()) {
    get_control()->fire();
  }
}



template <typename R>
template <ck::future_callback<R> F>
auto ck::future<R>::map(F f) -> ck::future<decltype(f(R{}))> {
  using T = decltype(f(R{}));
  // the future that gets resolved by the return value of f
  ck::future<T> c;

  // get a weak reference to the control structure of the new future
  auto c_ctrl = c.get_control();
  this->then([c_ctrl, this, f](R r) {
    // this future has resolved, so call the function and use it to resolve
    // the continuation future
    T t = f(move(r));

    auto ctrl = c_ctrl.get();
    if (ctrl) {
      ctrl->resolve(move(t));
    }
  });

  return move(c);
}

template <typename R>
void ck::future<R>::clear() {
  this->control.clear();
}