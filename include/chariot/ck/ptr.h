#pragma once

#include <template_lib.h>
#include "atom.h"

#ifdef KERNEL
#include <printk.h>
#else
#include <chariot.h>
#endif

namespace ck {


  // Takes in a default deleter. unique_ptr_deleters operator will be invoked by
  // default.
  template <typename T>
  class unique_ptr {
   private:
    T* m_ptr = nullptr;

   public:
    // Safely constructs resource. Operator new is called by the user. Once
    // constructed the unique_ptr will own the resource. std::move is used because
    // it is used to indicate that an object may be moved from other resource.
    explicit unique_ptr(T* raw_resource) noexcept : m_ptr(move(raw_resource)) {
    }
    unique_ptr(nullptr_t) : m_ptr(nullptr) {
    }

    unique_ptr() noexcept : m_ptr(nullptr) {
    }

    // destroys the resource when object goes out of scope. This will either call
    // the default delete operator or a user-defined one.
    ~unique_ptr() noexcept {
      delete m_ptr;
    }
    // Disables the copy/ctor and copy assignment operator. We cannot have two
    // copies exist or it'll bypass the RAII concept.
    unique_ptr(const unique_ptr<T>&) noexcept = delete;
    unique_ptr& operator=(const unique_ptr&) noexcept = delete;

    template <typename U>
    unique_ptr(const unique_ptr<U>&) = delete;

    template <typename U>
    unique_ptr& operator=(const unique_ptr<U>&) = delete;

   public:
    // Allows move-semantics. While the unique_ptr cannot be copied it can be
    // safely moved.
    unique_ptr(unique_ptr<T>&& move) noexcept {
      move.swap(*this);
    }
    // ptr = std::move(resource)
    unique_ptr<T>& operator=(unique_ptr<T>&& move) noexcept {
      move.swap(*this);
      return *this;
    }

    template <typename U>
    unique_ptr(unique_ptr<U>&& other) noexcept {
      if (this != static_cast<void*>(&other)) {
        delete m_ptr;
        m_ptr = other.leak_ptr();
      }
    }

    template <typename U>
    unique_ptr& operator=(unique_ptr<U>&& other) {
      if (this != static_cast<void*>(&other)) {
        delete m_ptr;
        m_ptr = other.leak_ptr();
      }
      return *this;
    }

    unique_ptr<T>& operator=(T* ptr) {
      if (m_ptr != ptr) delete m_ptr;
      m_ptr = ptr;
      return *this;
    }

    explicit operator bool() const noexcept {
      return this->m_ptr;
    }
    // releases the ownership of the resource. The user is now responsible for
    // memory clean-up.
    T* release() noexcept {
      return exchange(m_ptr, nullptr);
    }
    // returns a pointer to the resource
    T* get() const noexcept {
      return m_ptr;
    }

    // swaps the resources
    void swap(unique_ptr<T>& resource_ptr) noexcept {
      ::swap(m_ptr, resource_ptr.m_ptr);
    }
    // replaces the resource. the old one is destroyed and a new one will take
    // it's place.
    void reset(T* resource_ptr) noexcept(false) {
      // ensure a invalid resource is not passed or program will be terminated
      if (resource_ptr == nullptr) {
        panic("An invalid pointer was passed, resources will not be swapped");
      }

      delete m_ptr;

      m_ptr = nullptr;

      ::swap(m_ptr, resource_ptr);
    }

    T* leak_ptr() {
      T* leaked_ptr = m_ptr;
      m_ptr = nullptr;
      return leaked_ptr;
    }

   public:
    // overloaded operators
    T* operator->() const noexcept {
      return this->m_ptr;
    }
    T& operator*() const noexcept {
      return *this->m_ptr;
    }
    // May be used to check for nullptr
  };

  template <typename T, typename... Args>
  unique_ptr<T> make_unique(Args&&... args) {
    return unique_ptr<T>(new T(forward<Args>(args)...));
  }




#define SHARED_ASSERT(x) assert(x)
#define ASSERT(x) assert(x)

  template <class T>
  constexpr auto call_will_be_destroyed_if_present(T* object)
      -> decltype(object->will_be_destroyed(), TrueType{}) {
    object->will_be_destroyed();
    return {};
  }

  constexpr auto call_will_be_destroyed_if_present(...) -> FalseType {
    return {};
  }

  template <class T>
  constexpr auto call_one_ref_left_if_present(T* object)
      -> decltype(object->one_ref_left(), TrueType{}) {
    object->one_ref_left();
    return {};
  }

  constexpr auto call_one_ref_left_if_present(...) -> FalseType {
    return {};
  }


  template <typename T>
  class refcounted {
   public:
    void ref_retain() {
      assert(m_ref_count);
      m_ref_count.store(m_ref_count.load() + 1);
    }

    int ref_count() const {
      return m_ref_count;
    }
    void ref_release() {
      deref_base();
      if (m_ref_count == 0) {
        call_will_be_destroyed_if_present(static_cast<T*>(this));
        delete static_cast<T*>(this);
      } else if (m_ref_count == 1) {
        call_one_ref_left_if_present(static_cast<T*>(this));
      }
    }

   protected:
    refcounted() {
    }
    ~refcounted() { /* assert(m_ref_count == 0); */
    }

    void deref_base() {
      assert(m_ref_count);
      m_ref_count.store(m_ref_count.load() - 1);
    }


    atom<unsigned> m_ref_count = 1;
  };



  template <typename T>
  inline void retain_if_not_null(T* ptr) {
    if (ptr) ptr->ref_retain();
  }
  template <typename T>
  inline void release_if_not_null(T* ptr) {
    if (ptr) ptr->ref_release();
  }
  template <typename T>
  class ref {
   public:
    enum AdoptTag { Adopt };

    ref() {
    }
    ref(const T* ptr) : m_ptr(const_cast<T*>(ptr)) {
      retain_if_not_null(m_ptr);
    }
    ref(const T& object) : m_ptr(const_cast<T*>(&object)) {
      m_ptr->ref();
    }
    ref(AdoptTag, T& object) : m_ptr(&object) {
    }
    ref(ref&& other) : m_ptr(other.leak_ref()) {
    }
    template <typename U>
    ref(ref<U>&& other) : m_ptr(static_cast<T*>(other.leak_ref())) {
    }
    ref(const ref& other) : m_ptr(const_cast<T*>(other.ptr())) {
      retain_if_not_null(m_ptr);
    }
    template <typename U>
    ref(const ref<U>& other) : m_ptr(static_cast<T*>(const_cast<U*>(other.ptr()))) {
      retain_if_not_null(m_ptr);
    }
    ~ref() {
      clear();
      m_ptr = NULL;
      if constexpr (sizeof(T*) == 8)
        m_ptr = (T*)(NULL);
      else
        m_ptr = (T*)(NULL);
    }
    ref(nullptr_t) {
    }

    template <typename U>
    ref(const unique_ptr<U>&) = delete;
    template <typename U>
    ref& operator=(const unique_ptr<U>&) = delete;

    template <typename U>
    void swap(ref<U>& other) {
      ::swap(m_ptr, other.m_ptr);
    }

    ref& operator=(ref&& other) {
      ref tmp = move(other);
      swap(tmp);
      return *this;
    }

    template <typename U>
    ref& operator=(ref<U>&& other) {
      ref tmp = move(other);
      swap(tmp);
      return *this;
    }

    ref& operator=(const ref& other) {
      ref tmp = other;
      swap(tmp);
      return *this;
    }

    template <typename U>
    ref& operator=(const ref<U>& other) {
      ref tmp = other;
      swap(tmp);
      return *this;
    }

    ref& operator=(const T* ptr) {
      ref tmp = ptr;
      swap(tmp);
      return *this;
    }

    ref& operator=(const T& object) {
      ref tmp = object;
      swap(tmp);
      return *this;
    }

    ref& operator=(nullptr_t) {
      clear();
      return *this;
    }

    void clear() {
      release_if_not_null(m_ptr);
      m_ptr = nullptr;
    }

    bool operator!() const {
      return !m_ptr;
    }

    [[nodiscard]] T* leak_ref() {
      return exchange(m_ptr, nullptr);
    }

    T* ptr() {
      return m_ptr;
    }
    T* get() {
      return m_ptr;
    }
    const T* ptr() const {
      return m_ptr;
    }

    T* operator->() {
      if (!m_ptr) {
        panic("m_ptr error");
      }
      return m_ptr;
    }

    const T* operator->() const {
      ASSERT(m_ptr);
      return m_ptr;
    }

    T& operator*() {
      ASSERT(m_ptr);
      return *m_ptr;
    }

    const T& operator*() const {
      ASSERT(m_ptr);
      return *m_ptr;
    }

    operator const T*() const {
      return m_ptr;
    }
    operator T*() {
      return m_ptr;
    }

    operator bool() {
      return !!m_ptr;
    }

    bool operator==(nullptr_t) const {
      return !m_ptr;
    }
    bool operator!=(nullptr_t) const {
      return m_ptr;
    }

    bool operator==(const ref& other) const {
      return m_ptr == other.m_ptr;
    }
    bool operator!=(const ref& other) const {
      return m_ptr != other.m_ptr;
    }

    bool operator==(ref& other) {
      return m_ptr == other.m_ptr;
    }
    bool operator!=(ref& other) {
      return m_ptr != other.m_ptr;
    }

    bool operator==(const T* other) const {
      return m_ptr == other;
    }
    bool operator!=(const T* other) const {
      return m_ptr != other;
    }

    bool operator==(T* other) {
      return m_ptr == other;
    }
    bool operator!=(T* other) {
      return m_ptr != other;
    }

    bool is_null() const {
      return !m_ptr;
    }

   private:
    T* m_ptr = nullptr;
  };



  // static cast of ck::ref
  template <class T, class U>
  ck::ref<T> static_pointer_cast(const ck::ref<U>& ptr)  // never throws
  {
    return ck::ref<T>(ptr, static_cast<typename ck::ref<T>::element_type*>(ptr.get()));
  }

  // dynamic cast of ck::ref
  template <class T, class U>
  ck::ref<T> dynamic_pointer_cast(const ck::ref<U>& ptr)  // never throws
  {
    T* p = dynamic_cast<typename ck::ref<T>::element_type*>(ptr.get());
    if (NULL != p) {
      return ck::ref<T>(ptr, p);
    } else {
      return ck::ref<T>();
    }
  }

  template <typename T, typename... Args>
  ck::ref<T> make_ref(Args&&... args) {
    return ck::ref<T>(ck::ref<T>::AdoptTag::Adopt, *new T(forward<Args>(args)...));
  }



};  // namespace ck




// comparaison operators
template <class T, class U>
bool operator==(const ck::ref<T>& l,
                const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() == r.get());
}
template <class T, class U>
bool operator!=(const ck::ref<T>& l,
                const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() != r.get());
}
template <class T, class U>
bool operator<=(const ck::ref<T>& l,
                const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() <= r.get());
}
template <class T, class U>
bool operator<(const ck::ref<T>& l,
               const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() < r.get());
}
template <class T, class U>
bool operator>=(const ck::ref<T>& l,
                const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() >= r.get());
}
template <class T, class U>
bool operator>(const ck::ref<T>& l,
               const ck::ref<U>& r) throw()  // never throws
{
  return (l.get() > r.get());
}
