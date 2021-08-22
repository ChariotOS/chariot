#pragma once


#include <ck/utility.h>
#include <ck/func.h>
#include <ck/option.h>

namespace ck {


  template <typename R>
  struct future_control : public ck::weakable<future_control<R>> {
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
      assert(!value.has_value());
      value = move(v);

      if (on_ready) {
        fire();
      }
    }
  };

  template <typename R>
  class future {
   public:
    typedef R value_type;

    future() noexcept;
    ~future();
    future(const future<R>& other) : control(other.control) {}                             // copy
    future(future<R>&& other) : control(other.get_control()) { other.control = nullptr; }  // move


    future<R>& operator=(future<R>& other) {
      control = other.get_control();
      return *this;
    }

    future<R>& operator=(future<R>&& other) {
      control = other.get_control();
      other.control = nullptr;
      return *this;
    }



    void then(ck::func<void(R)> f);
    // upon resolution, map the value to a new value and return that value as a future
    template <typename F>
    auto map(F f) -> ck::future<decltype(f(R{}))>;
    void resolve(R&& v) { get_control()->resolve(move(v)); }
    bool resolved(void) const { return get_control()->value.has_value() || fired(); }
    bool fired(void) const { return get_control()->fired; }
    // get the control as a refcount, as if you are calling this method, the future is still alive.
    ck::ref<future_control<R>> get_control(void);

    //  protected:
    ck::ref<future_control<R>> control;
  };

}  // namespace ck




/// Implementation for the future

template <typename R>
ck::future<R>::future() noexcept {}

template <typename R>
ck::future<R>::~future() {
  control.clear();
}

template <typename R>
ck::ref<ck::future_control<R>> ck::future<R>::get_control() {
  if (!control) {
    control = ck::make_ref<future_control<R>>();
  }
  return control;
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
    return;
  }
}



template <typename R>
template <typename F>
auto ck::future<R>::map(F f) -> ck::future<decltype(f(R{}))> {
  using T = decltype(f(R{}));
  // the future that gets resolved by the return value of f
  ck::future<T> c;

  // get a weak reference to the control structure of the new future
  ck::weak_ref<future_control<T>> c_ctrl = c.get_control();
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