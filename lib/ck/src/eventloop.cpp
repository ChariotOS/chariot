#include <ck/eventloop.h>
#include <ck/fsnotifier.h>
#include <ck/map.h>
#include <ck/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <chariot.h>

#include <chariot/awaitfs_types.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include <ck/ptr.h>
#include <ck/event.h>
#include <ck/rand.h>
#include <ck/tuple.h>
#include <ck/time.h>


static volatile bool currently_running_defered_functions = false;
static ck::vec<ck::tuple<const char *, ck::func<void(void)>>> s_defered;

void ck::eventloop::defer(ck::func<void(void)> cb) {
  s_defered.push(ck::tuple((const char *)nullptr, move(cb)));
  // printf("adding. now %llu\n", s_defered.size());
}

void ck::eventloop::defer_unique(const char *name, ck::func<void(void)> cb) {
  for (auto &existing : s_defered) {
    const char *existing_name = existing.first();
    if (existing_name == NULL) continue;
    if (strcmp(existing_name, name) == 0) {
      return;
    }
  }
  s_defered.push(ck::tuple(name, move(cb)));

  // printf("uniquely defered '%s'\n", name);
}
static ck::HashTable<ck::fsnotifier *> s_notifiers;
static ck::HashTable<ck::timer *> s_timers;


static size_t current_ms() { return sysbind_gettime_microsecond() / 1000; }

static ck::timer *next_timer(void) {
  // TODO: take a lock
  ck::timer *n = NULL;

  for (auto *t : s_timers) {
    if (n == NULL) {
      n = t;
    } else {
      if (t->next_fire() < n->next_fire()) {
        n = t;
      }
    }
  }

  return n;
}

ck::eventloop *active_eventloop = NULL;

ck::eventloop::eventloop(void) {}
ck::eventloop::~eventloop(void) {}

void ck::eventloop::exit(int ec) {
  if (active_eventloop == NULL) return;
  active_eventloop->m_exit_code = ec;
  active_eventloop->m_finished = true;
}



void ck::eventloop::start(void) {
  auto old = active_eventloop;
  active_eventloop = this;
  for (;;) {
    if (m_finished) break;
    run_fibers();
    pump();
    dispatch();

    if (m_fibers.size() == 0 && s_notifiers.size() == 0 && s_defered.size() == 0) {
      this->exit();
    }
  }
  m_finished = false;
  active_eventloop = old;
}




void ck::eventloop::block_on(ck::func<void(void)> fn) {
  // we only exit if all fibers are dead.
  auto fiber = ck::make_ref<ck::fiber>([=](ck::fiber *fiber) mutable {
    // fiber->on_exit = [](ck::fiber *f) {
    //   assert(f->is_done());
    //   ck::eventloop::current()->exit();
    // };

    fn();
  });


  fiber->set_ready();
  add_fiber(fiber);

  start();
}

void ck::eventloop::add_fiber(ck::ref<ck::fiber> f) {
  f->set_ready();
  // add the fiber to the pool
  m_fibers.push(f);
}

int awaitfs(struct await_target *fds, int nfds, int flags, long long timeout_time) {
  int res = 0;
  do {
    res = sysbind_awaitfs(fds, nfds, flags, timeout_time);
  } while (res == -EINTR);
  return errno_wrap(res);
}

void ck::eventloop::run_deferred() {
  while (s_defered.size() != 0) {
    auto this_defered = move(s_defered);
    s_defered.clear();

    for (auto &cb : this_defered)
      cb.second()();
  }
}

ck::pair<ck::timer *, long long> ck::eventloop::check_timers() {
  long long timeout = -1;
  long long now = current_ms();

  ck::timer *nt = nullptr;
  while (1) {
    nt = next_timer();
    if (nt == NULL) break;

    timeout = nt->next_fire();
    if (timeout > now) break;

    if (now > timeout <= now) {
      nt->trigger();
      nt = NULL;
      timeout = -1;
    }
  }
  return {nt, timeout};
}


static long nonce = 0;
void ck::eventloop::pump(void) {
  // step 1: run any defered functions
  run_deferred();

  // step 2: check the timers for any pending timer ticks. If there are any, run them. When no more
  // pending timer ticks are avail, pick the closest timeout as the timeout for the awaitfs call
  auto [nt, timeout] = check_timers();

  // step 3: construct an array of await_targets for the awaitfs call
  ck::vec<struct await_target> targs;
  targs.ensure_capacity(s_notifiers.size());
  auto add_fd = [&](int fd, int mask, ck::object *obj) {
    struct await_target targ = {0};
    targ.fd = fd;
    targ.awaiting = mask;
    targ.priv = (void *)obj;
    targs.push(move(targ));
  };

  for (auto &notifier : s_notifiers) {
    int fd = notifier->fd();
    int event = notifier->ev_mask();
    add_fd(fd, event, notifier);
  }


  /* Shuffle the target order */
  int n = targs.size();
  for (int i = 0; i < n; i++) {
    int j = i + ::rand() / (RAND_MAX / (n - i) + 1);
    if (j == i) continue;
    swap(targs[i], targs[j]);
  }

  if (targs.size() > 0 || timeout != -1) {
    // printf("<\n");
    int index = awaitfs(targs.data(), targs.size(), 0, timeout);
    // printf(">\n");
    if (index >= 0) {
      auto occ = targs[index].occurred;
      if (occ & AWAITFS_READ) {
        auto event = new ck::event();
        event->type = CK_EVENT_READ;
        post_event(*(ck::object *)targs[index].priv, event);
      }

      if (occ & AWAITFS_WRITE) {
        auto event = new ck::event();
        event->type = CK_EVENT_WRITE;
        post_event(*(ck::object *)targs[index].priv, event);
      }

    } else {
      if (errno == ETIMEDOUT) {
        if (nt != NULL) {
          nt->trigger();
        }
      }
    }
  }
}

void ck::eventloop::post_event(ck::object &obj, ck::event *ev) { m_pending.push({obj, ev}); }

void ck::eventloop::dispatch(void) {
  for (auto &ev : m_pending) {
    ev.obj.event(*ev.ev);
    delete ev.ev;
  }
  m_pending.clear();
}

void ck::eventloop::run_fibers() {
  ck::vec<ck::ref<ck::fiber>> finished_fibers;
  ck::vec<ck::ref<ck::fiber>> to_run;



  if (m_fibers.size() > 0) {
    while (1) {
      /* Shuffle the fiber order */
      int n = m_fibers.size();
      for (int i = 0; i < n; i++) {
        int j = i + ::rand() / (RAND_MAX / (n - i) + 1);
        if (j == i) continue;
        swap(m_fibers[i], m_fibers[j]);
      }

      // runnable fibers
      for (auto &fiber : m_fibers) {
        if (fiber->state() == ck::FiberState::Ready) {
          to_run.push(fiber);
        }
      }

      if (to_run.size() == 0) break;

      for (auto &fiber : to_run) {
        if (!fiber->is_done() && fiber->state() == ck::FiberState::Ready) fiber->resume();
        if (fiber->is_done()) {
          finished_fibers.push(fiber);
        }
      }

      to_run.clear();

      for (auto &f : finished_fibers) {
        m_fibers.remove_first_matching([&](auto other) {
          return other == f;
        });
      }
      finished_fibers.clear();
    }
  }
}


ck::eventloop *ck::eventloop::current(void) { return active_eventloop; }


void ck::eventloop::register_notifier(ck::fsnotifier &n) {
  // printf("register notifier %p\n", &n);
  s_notifiers.set(&n);
}


void ck::eventloop::deregister_notifier(ck::fsnotifier &n) {
  // printf("dregister notifier %p\n", &n);
  s_notifiers.remove(&n);
}

/////////////////////////////////////////////////////////////////////////////////////


ck::ref<ck::timer> ck::timer::make_interval(int ms, ck::func<void()> cb) {
  auto t = ck::make_ref<ck::timer>();
  t->on_tick = move(cb);
  t->start(ms, true);
  return t;
}

ck::ref<ck::timer> ck::timer::make_timeout(int ms, ck::func<void()> cb) {
  auto t = ck::make_ref<ck::timer>();
  t->on_tick = move(cb);
  t->start(ms, false);
  return t;
}


void ck::timer::start(uint64_t ms, bool repeat) {
  m_interval = ms;
  m_next_fire = current_ms() + m_interval;
  active = true;
  this->repeat = repeat;
  // TODO: take a lock
  s_timers.set(this);
}

void ck::timer::trigger(void) {
  if (active) {
    if (!repeat) {
      stop();
    } else {
      m_next_fire = current_ms() + m_interval;
    }
  }
  // do this part last cause it might take a long time
  on_tick();
}

ck::timer::~timer(void) {
  if (active) stop();
}

void ck::timer::stop(void) {
  if (active) {
    // TODO: take a lock
    active = false;
    s_timers.remove(this);
  }
}



/////////////////////////////////////////////////////////////////////////////////////



ck::fsnotifier::fsnotifier(int fd, int event_mask) : m_fd(fd), m_ev_mask(event_mask) { set_active(true); }


ck::fsnotifier::~fsnotifier(void) { set_active(false); }

void ck::fsnotifier::set_active(bool a) {
  if (a) {
    ck::eventloop::register_notifier(*this);
  } else {
    ck::eventloop::deregister_notifier(*this);
  }
}


bool ck::fsnotifier::event(const ck::event &ev) {
  on_event(ev.type);
  return true;
}
