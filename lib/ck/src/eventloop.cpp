#include <ck/eventloop.h>
#include <ck/fsnotifier.h>
#include <ck/map.h>
#include <ck/timer.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <chariot/awaitfs_types.h>
#include <sys/sysbind.h>
#include <sys/syscall.h>
#include "ck/event.h"
#include <ck/rand.h>



static ck::vec<ck::func<void(void)>> s_defered;
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

void ck::eventloop::defer(ck::func<void(void)> cb) { s_defered.push(move(cb)); }

ck::eventloop::eventloop(void) {}

ck::eventloop::~eventloop(void) {}

void ck::eventloop::exit(void) {
  if (active_eventloop == NULL) return;
  active_eventloop->m_finished = true;
}



void ck::eventloop::start(void) {
  auto old = active_eventloop;
  active_eventloop = this;
  for (;;) {
    if (m_finished) break;
    pump();
    dispatch();
  }
  m_finished = false;
  active_eventloop = old;
}


int awaitfs(struct await_target *fds, int nfds, int flags, long long timeout_time) {
  int res = 0;
  do {
    res = sysbind_awaitfs(fds, nfds, flags, timeout_time);
  } while (res == -EINTR);
  return errno_wrap(res);
}



void ck::eventloop::pump(void) {
  for (auto &cb : s_defered) {
    cb();
  }
  s_defered.clear();


  ck::vec<struct await_target> targs;
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

  long long timeout = -1;
  auto nt = next_timer();

  if (nt != NULL) {
    timeout = nt->next_fire();
  }


	/* Shuffle the target order */
	int n = targs.size();
	for (int i = 0; i < n; i++) {
		int j = i + ::rand() / (RAND_MAX / (n - i) + 1);
		swap(targs[i], targs[j]);
	}

  int index = awaitfs(targs.data(), targs.size(), 0, timeout);
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

void ck::eventloop::post_event(ck::object &obj, ck::event *ev) { m_pending.empend(obj, ev); }

void ck::eventloop::dispatch(void) {
  // printf("dispatch: %d events\n", m_pending.size());
  // just loop through each event that needs to be dispatched and send them to
  // the clients
  for (auto &ev : m_pending) {
    ev.obj.event(*ev.ev);
    delete ev.ev;
  }
  m_pending.clear();
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
  auto t = new ck::timer();
  t->on_tick = move(cb);
  t->start(ms, true);
  return t;
}

ck::ref<ck::timer> ck::timer::make_timeout(int ms, ck::func<void()> cb) {
  auto t = new ck::timer();
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
    s_timers.remove(this);
    active = false;
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
