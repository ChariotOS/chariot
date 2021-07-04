#pragma once

#include <ck/fsnotifier.h>
#include <ck/func.h>
#include <ck/object.h>
#include <ck/ptr.h>
#include <ck/vec.h>
#include <ck/pair.h>



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


    static ck::eventloop *current(void);

    static void register_notifier(ck::fsnotifier &);
    static void deregister_notifier(ck::fsnotifier &);

    // cause the eventlopp to exit
    static void exit(void);

    static void defer(ck::func<void(void)> cb);

    static void defer_unique(const char *name, ck::func<void(void)> cb);

    void run_deferred();
    ck::pair<ck::timer *, long long> check_timers();


   private:
    bool m_finished = false;


    struct pending_event {
      inline pending_event(ck::object &obj, ck::event *ev) : obj(obj), ev(ev) {}
      ~pending_event() = default;

      pending_event(pending_event &&) = default;

      ck::object &obj;
      ck::event *ev;
    };

    ck::vec<pending_event> m_pending;
  };
};  // namespace ck
