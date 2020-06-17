#pragma once

#include <ck/func.h>
#include <ck/object.h>
#include <chariot/awaitfs_types.h>

namespace ck {


  /**
   * functionality implemented in ck/eventloop.cpp
   */
  class fsnotifier : public ck::object {
   public:
    ck::func<void()> on_read;
    ck::func<void()> on_write;


    fsnotifier(int fd, int event_mask = 0);
    ~fsnotifier(void);


    void set_event_mask(int ev);


    void set_active(bool);


    inline int fd(void) { return m_fd; }
    inline int ev_mask(void) { return m_ev_mask; }


    virtual bool event(const ck::event&) override;

   private:
    int m_fd = -1;
    int m_ev_mask = 0;
  };
};  // namespace ck
