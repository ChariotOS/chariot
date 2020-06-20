#include <ck/eventloop.h>
#include <ck/fsnotifier.h>
#include <ck/map.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <chariot/awaitfs_types.h>
#include <sys/syscall.h>
#include "ck/event.h"
#include "ck/event.h"

static ck::HashTable<ck::fsnotifier *> s_notifiers;

ck::eventloop *active_eventloop = NULL;

ck::eventloop::eventloop(void) {
	/*
  if (active_eventloop != NULL) {
    panic(
        "Attempt to construct an event loop despite there already being one in "
        "existence\n");
    exit(EXIT_FAILURE);
  }
  active_eventloop = this;
	*/
}

ck::eventloop::~eventloop(void) {
	/*
	active_eventloop = NULL;
	*/
}

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


int awaitfs(struct await_target *fds, int nfds, int flags, unsigned long timeout_time) {
  return errno_syscall(SYS_awaitfs, fds, nfds, flags, timeout_time);
}


void ck::eventloop::pump(void) {
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


  int index = awaitfs(targs.data(), targs.size(), 0, 0);
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

  }
}

void ck::eventloop::post_event(ck::object &obj, ck::event *ev) {
  m_pending.empend(obj, ev);
}

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



ck::fsnotifier::fsnotifier(int fd, int event_mask)
    : m_fd(fd), m_ev_mask(event_mask) {
  set_active(true);
}


ck::fsnotifier::~fsnotifier(void) { set_active(false); }

void ck::fsnotifier::set_active(bool a) {
	if (a) {
		ck::eventloop::register_notifier(*this);
	} else {
		ck::eventloop::deregister_notifier(*this);
	}
}


bool ck::fsnotifier::event(const ck::event & ev) {
	on_event(ev.type);
	return true;
}
