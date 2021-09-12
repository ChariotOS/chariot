#pragma once

#include <ck/fsnotifier.h>
#include <ck/func.h>
#include <ck/object.h>
#include <ck/ptr.h>
#include <ck/vec.h>
#include <ck/pair.h>
#include <ck/set.h>
#include <ck/fiber.h>
#include <ck/future.h>

// TODO: exit codes!
#define ASYNC_MAIN(body)   \
  ck::eventloop __EV;      \
  __EV.block_on([&] body); \
  return __EV.exit_code();

#define ASYNC_BODY(body) \
  ck::eventloop __EV;    \
  __EV.block_on([&] body);

namespace ck {
  class timer;



  class eventloop {
   public:
    eventloop(void);
    ~eventloop(void);


    /**
     * start the eventloop, only to return once we are told.
     */
    void start(void);


    /**
     * Find all events that need to be dispatched
     */
    void pump(void);


    /**
     * prepare an event to be sent to an object
     */
    void post_event(ck::object &obj, ck::event *ev);

    /**
     * loop through all the posted events and actually evaluate them
     */
    void dispatch(void);

    void run_fibers(void);


    static ck::eventloop *current(void);

    static void register_notifier(ck::fsnotifier &);
    static void deregister_notifier(ck::fsnotifier &);

    // cause the eventlopp to exit
    static void exit(int exit_code = 0);

    static void defer(ck::func<void(void)> cb);

    static void defer_unique(const char *name, ck::func<void(void)> cb);

    void run_deferred();
    ck::pair<ck::timer *, long long> check_timers();


    void block_on(ck::func<void(void)> cb);
    void add_fiber(ck::ref<ck::fiber> f);

    int exit_code(void) const { return m_exit_code; }


   private:
    bool m_finished = false;
    bool m_exit_on_no_fibers = false;
    int m_exit_code = 0;

    struct pending_event {
      inline pending_event(ck::object &obj, ck::event *ev) : obj(obj), ev(ev) {}
      ~pending_event() = default;

      pending_event(pending_event &&) = default;

      ck::object &obj;
      ck::event *ev;
    };

    ck::vec<ck::ref<ck::fiber>> m_fibers;
    ck::vec<pending_event> m_pending;
  };


  inline void spawn(ck::func<void(void)> f) {
    ck::eventloop::current()->add_fiber(ck::make_ref<ck::fiber>([=](ck::fiber *fiber) mutable {
      f();
    }));
  }

  template <typename T>
  async(T) spawn(ck::func<T(void)> f) {
    ck::future<T> fut;
    auto ctrl = fut.get_control();
    auto fiber = ck::make_ref<ck::fiber>([f, ctrl](ck::fiber *fiber) mutable {
      ctrl->resolve(f());
    });

    ck::eventloop::current()->add_fiber(fiber);

    return fut;
  }

};  // namespace ck
