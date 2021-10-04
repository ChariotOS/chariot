#pragma once

#include <ck/func.h>
#include <ck/object.h>
#include <stdint.h>

namespace ck {

  /*
   * This `timer` class is a "best shot" implementation of a timer. This
   * means timeouts will be "at least" as long as they need to be, but might
   * be longer than expected due to scheduling, but that's kinda just how
   * scheduling works I guess
   */
  class timer final : public ck::object {
    CK_OBJECT(ck::timer);

   public:
    using ref = ck::ref<ck::timer>;
    ~timer();
    static ck::ref<ck::timer> make_interval(int ms, ck::func<void()>);
    static ck::ref<ck::timer> make_timeout(int ms, ck::func<void()>);


    void start(uint64_t interval, bool repeat);
    void stop();
    void trigger();

    inline long long next_fire(void) { return m_next_fire; }
    inline bool running() { return active; }
    ck::func<void()> on_tick;

   protected:
    long long m_interval = -1;
    long long m_next_fire = -1;
    bool repeat = false;
    bool active = false;
  };
}  // namespace ck
