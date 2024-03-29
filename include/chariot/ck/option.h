#pragma once


#include <ck/func.h>

#ifdef KERNEL
#include <printf.h>
#include <types.h>
#else
#include <chariot.h>
#include <stdint.h>
#include <assert.h>
#endif

namespace ck {
#define ALWAYS_INLINE [[gnu::always_inline]] inline

  template <typename T>
  class alignas(T) option {
   public:
    option() {}

    option(const T& value) : m_has_value(true) { new (&m_storage) T(value); }

    template <typename U>
    option(const U& value) : m_has_value(true) {
      new (&m_storage) T(value);
    }

    option(T&& value) : m_has_value(true) { new (&m_storage) T(move(value)); }

    option(option&& other) : m_has_value(other.m_has_value) {
      if (other.has_value()) {
        new (&m_storage) T(other.take());
        other.m_has_value = false;
      }
    }

    option(const option& other) : m_has_value(other.m_has_value) {
      if (m_has_value) {
        new (&m_storage) T(other.value_without_consume_state());
      }
    }

    option& operator=(const option& other) {
      if (this != &other) {
        clear();
        m_has_value = other.m_has_value;
        if (m_has_value) {
          new (&m_storage) T(other.unwrap());
        }
      }
      return *this;
    }

    option& operator=(option&& other) {
      if (this != &other) {
        clear();
        m_has_value = other.m_has_value;
        if (other.has_value()) new (&m_storage) T(other.take());
      }
      return *this;
    }

    ALWAYS_INLINE ~option() { clear(); }

    ALWAYS_INLINE void clear() {
      if (m_has_value) {
        unwrap().~T();
        m_has_value = false;
      }
    }

    ALWAYS_INLINE bool has_value() const { return m_has_value; }
    ALWAYS_INLINE operator bool(void) const { return m_has_value; }


    template <typename Fn>
    ALWAYS_INLINE auto map(Fn cb) -> ck::option<decltype(cb(T{}))> {
      if (m_has_value) {
        return cb(unwrap());
      }
      return {};
    }

    T take() {
      assert(m_has_value);

      T released_value = move(unwrap());
      unwrap().~T();
      m_has_value = false;
      return released_value;
    }
    ALWAYS_INLINE T or_default(const T fallback) {
      if (m_has_value) return take();
      return fallback;
    }

    ALWAYS_INLINE T& unwrap() {
      assert(m_has_value);
      return *reinterpret_cast<T*>(&m_storage);
    }

    ALWAYS_INLINE const T& unwrap() const {
      assert(m_has_value);
      return value_without_consume_state();
    }


    ALWAYS_INLINE T& unwrap_or(const T& fallback) const {
      if (m_has_value) return unwrap();
      return fallback;
    }



   private:
    // Call when we don't want to alter the consume state
    ALWAYS_INLINE const T& value_without_consume_state() const {
      // ASSERT(m_has_value);
      return *reinterpret_cast<const T*>(&m_storage);
    }
    uint8_t m_storage[sizeof(T)]{0};
    bool m_has_value = false;
  };


#ifndef KERNEL


#define Some(val) ck::option<decltype(val)>(val)
#define None \
  {}

#endif

#undef ALWAYS_INLINE
}  // namespace ck
