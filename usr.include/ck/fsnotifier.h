#pragma once

#include <chariot/awaitfs_types.h>
#include <ck/func.h>
#include <ck/object.h>

namespace ck {


  /**
   * functionality implemented in ck/eventloop.cpp
   */
  class fsnotifier : public ck::object {
   public:
    ck::func<void(int event)> on_event;

    fsnotifier(int fd, int event_mask = 0);
    inline fsnotifier() {}
    ~fsnotifier(void);


    inline void init(int fd, int ev) {
      m_fd = fd;
      m_ev_mask = ev;
      set_active(true);
    }

    void set_event_mask(int ev) {
			m_ev_mask = ev;
		}

    void set_active(bool);


    inline int fd(void) { return m_fd; }
    inline int ev_mask(void) { return m_ev_mask; }


    virtual bool event(const ck::event&) override;

   private:
    int m_fd = -1;
    int m_ev_mask = 0;
  };
};  // namespace ck
