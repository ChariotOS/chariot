#pragma once

#include <asm.h>
#include <types.h>
#include "memory_order.h"

/**
 *  @brief Generic atomic type, primary class template.
 *
 *  @tparam T  Type to be made atomic, must be trivally copyable.
 */
template <typename T>
struct atom {
  using value_type = T;

 private:
  // Align 1/2/4/8/16-byte types to at least their size.
  static constexpr int _S_min_alignment = (sizeof(T) & (sizeof(T) - 1)) ||
                                                  sizeof(T) > 16
                                              ? 0
                                              : sizeof(T);

  static constexpr int _S_alignment = _S_min_alignment > alignof(T)
                                          ? _S_min_alignment
                                          : alignof(T);

  alignas(_S_alignment) T _M_i;

  static_assert(__is_trivially_copyable(T),
                "std::atomic requires a trivially copyable type");

  static_assert(sizeof(T) > 0,
                "Incomplete or zero-sized types are not supported");

 public:
  inline atom() noexcept = default;
  inline ~atom() noexcept = default;
  inline atom(const atom&) = default;
  inline atom& operator=(const atom&) = delete;
  inline atom& operator=(const atom&) volatile = delete;

  constexpr atom(T __i) noexcept : _M_i(__i) {}

  operator T() const noexcept { return load(); }

  operator T() const volatile noexcept { return load(); }

  T operator=(T __i) noexcept {
    store(__i);
    return __i;
  }

  T operator=(T __i) volatile noexcept {
    store(__i);
    return __i;
  }

  bool is_lock_free() const noexcept {
    // Produce a fake, minimally aligned pointer.
    return __atomic_is_lock_free(sizeof(_M_i),
                                 reinterpret_cast<void*>(-_S_alignment));
  }

  bool is_lock_free() const volatile noexcept {
    // Produce a fake, minimally aligned pointer.
    return __atomic_is_lock_free(sizeof(_M_i),
                                 reinterpret_cast<void*>(-_S_alignment));
  }

#if __cplusplus >= 201703L
  static constexpr bool is_always_lock_free =
      __atomic_always_lock_free(sizeof(_M_i), 0);
#endif

  inline void store(T __i, memory_order __m = memory_order_seq_cst) noexcept {
    __atomic_store(&(_M_i), &(__i), int(__m));
  }

  inline void store(T __i, memory_order __m = memory_order_seq_cst) volatile noexcept {
    __atomic_store(&(_M_i), &(__i), int(__m));
  }

  inline T load(memory_order __m = memory_order_seq_cst) const noexcept {
    alignas(T) unsigned char __buf[sizeof(T)];
    T* __ptr = reinterpret_cast<T*>(__buf);
    __atomic_load(&(_M_i), __ptr, int(__m));
    return *__ptr;
  }

  inline T load(memory_order __m = memory_order_seq_cst) const volatile noexcept {
    alignas(T) unsigned char __buf[sizeof(T)];
    T* __ptr = reinterpret_cast<T*>(__buf);
    __atomic_load(&(_M_i), __ptr, int(__m));
    return *__ptr;
  }

  inline T exchange(T __i, memory_order __m = memory_order_seq_cst) noexcept {
    alignas(T) unsigned char __buf[sizeof(T)];
    T* __ptr = reinterpret_cast<T*>(__buf);
    __atomic_exchange(&(_M_i), &(__i), __ptr, int(__m));
    return *__ptr;
  }

  inline T exchange(T __i, memory_order __m = memory_order_seq_cst) volatile noexcept {
    alignas(T) unsigned char __buf[sizeof(T)];
    T* __ptr = reinterpret_cast<T*>(__buf);
    __atomic_exchange(&(_M_i), &(__i), __ptr, int(__m));
    return *__ptr;
  }

  inline bool compare_exchange_weak(T& __e, T __i, memory_order __s,
                             memory_order __f) noexcept {
    return __atomic_compare_exchange(&(_M_i), &(__e), &(__i), true, int(__s),
                                     int(__f));
  }

  inline bool compare_exchange_weak(T& __e, T __i, memory_order __s,
                             memory_order __f) volatile noexcept {
    return __atomic_compare_exchange(&(_M_i), &(__e), &(__i), true, int(__s),
                                     int(__f));
  }

  inline bool compare_exchange_weak(T& __e, T __i,
                             memory_order __m = memory_order_seq_cst) noexcept {
    return compare_exchange_weak(__e, __i, __m, __cmpexch_failure_order(__m));
  }

  inline bool compare_exchange_weak(
      T& __e, T __i,
      memory_order __m = memory_order_seq_cst) volatile noexcept {
    return compare_exchange_weak(__e, __i, __m, __cmpexch_failure_order(__m));
  }

  inline bool compare_exchange_strong(T& __e, T __i, memory_order __s,
                               memory_order __f) noexcept {
    return __atomic_compare_exchange(&(_M_i), &(__e), &(__i), false, int(__s),
                                     int(__f));
  }

  inline bool compare_exchange_strong(T& __e, T __i, memory_order __s,
                               memory_order __f) volatile noexcept {
    return __atomic_compare_exchange(&(_M_i), &(__e), &(__i), false, int(__s),
                                     int(__f));
  }

  inline bool compare_exchange_strong(
      T& __e, T __i, memory_order __m = memory_order_seq_cst) noexcept {
    return compare_exchange_strong(__e, __i, __m, __cmpexch_failure_order(__m));
  }

  inline bool compare_exchange_strong(
      T& __e, T __i,
      memory_order __m = memory_order_seq_cst) volatile noexcept {
    return compare_exchange_strong(__e, __i, __m, __cmpexch_failure_order(__m));
  }
};
