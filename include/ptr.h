#ifndef __PTR_H__
#define __PTR_H__

#include <printk.h>
#include <template_lib.h>


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
  explicit unique_ptr(T* raw_resource) noexcept : m_ptr(move(raw_resource)) {}
  unique_ptr(nullptr_t) : m_ptr(nullptr) {}

  unique_ptr() noexcept : m_ptr(nullptr) {}

  // destroys the resource when object goes out of scope. This will either call
  // the default delete operator or a user-defined one.
  ~unique_ptr() noexcept { delete m_ptr; }
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
  unique_ptr(unique_ptr&& move) noexcept { move.swap(*this); }
  // ptr = std::move(resource)
  unique_ptr& operator=(unique_ptr&& move) noexcept {
    move.swap(*this);
    return *this;
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

  explicit operator bool() const noexcept { return this->m_ptr; }
  // releases the ownership of the resource. The user is now responsible for
  // memory clean-up.
  T* release() noexcept { return exchange(m_ptr, nullptr); }
  // returns a pointer to the resource
  T* get() const noexcept { return m_ptr; }
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
  T* operator->() const noexcept { return this->m_ptr; }
  T& operator*() const noexcept { return *this->m_ptr; }
  // May be used to check for nullptr
};

template <typename T, typename... Args>
unique_ptr<T> make_unique(Args&&... args) {
  return unique_ptr<T>(new T(forward<Args>(args)...));
}

#define SHARED_ASSERT(x) assert(x)

/**
 * @brief implementation of reference counter for the following minimal smart
 * pointer.
 *
 * ref_count is a container for the allocated pn reference counter.
 */
class ref_count {
 public:
  ref_count() : pn(NULL) {}
  ref_count(const ref_count& count) : pn(count.pn) {}
  /// @brief Swap method for the copy-and-swap idiom (copy constructor and swap
  /// method)
  void swap(ref_count& lhs) throw()  // never throws
  {
    ::swap(pn, lhs.pn);
  }
  /// @brief getter of the underlying reference counter
  long use_count(void) const throw()  // never throws
  {
    long count = 0;
    if (NULL != pn) {
      count = *pn;
    }
    return count;
  }
  /// @brief acquire/share the ownership of the pointer, initializing the
  /// reference counter
  template <class U>
  void acquire(U* p)  // may throw std::bad_alloc
  {
    if (NULL != p) {
      if (NULL == pn) {
        pn = new long(1);  // may throw std::bad_alloc
      } else {
        ++(*pn);
      }
    }
  }
  /// @brief release the ownership of the px pointer, destroying the object when
  /// appropriate
  template <class U>
  void release(U* p) throw()  // never throws
  {
    if (NULL != pn) {
      --(*pn);
      if (0 == *pn) {
        delete p;
        delete pn;
      }
      pn = NULL;
    }
  }

 public:
  long* pn;  //!< Reference counter
};

/**
 * @brief minimal implementation of smart pointer, a subset of the C++11
 * std::ref or boost::ref.
 *
 * ref is a smart pointer retaining ownership of an object through a
 * provided pointer, and sharing this ownership with a reference counter. It
 * destroys the object when the last shared pointer pointing to it is destroyed
 * or reset.
 */
template <class T>
class ref {
 public:
  /// The type of the managed object, aliased as member type
  typedef T element_type;

  /// @brief Default constructor
  ref(void) throw()
      :  // never throws
        px(NULL),
        pn() {}
  /// @brief Constructor with the provided pointer to manage
  explicit ref(T* p)
      :  // may throw std::bad_alloc
         // px(p), would be unsafe as acquire() may throw, which would call
         // release() in destructor
        pn() {
    acquire(p);  // may throw std::bad_alloc
  }
  /// @brief Constructor to share ownership. Warning : to be used for
  /// pointer_cast only ! (does not manage two separate <T> and <U> pointers)
  template <class U>
  ref(const ref<U>& ptr, T* p)
      :  // px(p), would be unsafe as acquire() may throw, which would call
         // release() in destructor
        pn(ptr.pn) {
    acquire(p);  // may throw std::bad_alloc
  }
  /// @brief Copy constructor to convert from another pointer type
  template <class U>
  ref(const ref<U>& ptr) throw()
      :  // never throws (see comment below)
         // px(ptr.px),
        pn(ptr.pn) {
    SHARED_ASSERT(
        (NULL == ptr.px) ||
        (0 != ptr.pn.use_count()));  // must be coherent : no allocation allowed
                                     // in this path
    acquire(static_cast<typename ref<T>::element_type*>(
        ptr.px));  // will never throw std::bad_alloc
  }
  /// @brief Copy constructor (used by the copy-and-swap idiom)
  ref(const ref& ptr) throw()
      :  // never throws (see comment below)
         // px(ptr.px),
        pn(ptr.pn) {
    SHARED_ASSERT(
        (NULL == ptr.px) ||
        (0 != ptr.pn.use_count()));  // must be cohérent : no allocation allowed
                                     // in this path
    acquire(ptr.px);  // will never throw std::bad_alloc
  }
  /// @brief Assignment operator using the copy-and-swap idiom (copy constructor
  /// and swap method)
  ref& operator=(ref ptr) throw()  // never throws
  {
    swap(ptr);
    return *this;
  }
  /// @brief the destructor releases its ownership
  ~ref(void) throw()  // never throws
  {
    release();
  }
  /// @brief this reset releases its ownership
  void reset(void) throw()  // never throws
  {
    release();
  }
  /// @brief this reset release its ownership and re-acquire another one
  void reset(T* p)  // may throw std::bad_alloc
  {
    SHARED_ASSERT((NULL == p) || (px != p));  // auto-reset not allowed
    release();
    acquire(p);  // may throw std::bad_alloc
  }

  /// @brief Swap method for the copy-and-swap idiom (copy constructor and swap
  /// method)
  void swap(ref& lhs) throw()  // never throws
  {
    ::swap(px, lhs.px);
    pn.swap(lhs.pn);
  }

  // reference counter operations :
  operator bool() const throw()  // never throws
  {
    return (0 < pn.use_count());
  }
  bool unique(void) const throw()  // never throws
  {
    return (1 == pn.use_count());
  }
  long use_count(void) const throw()  // never throws
  {
    return pn.use_count();
  }

  // underlying pointer operations :
  T& operator*() const throw()  // never throws
  {
    SHARED_ASSERT(NULL != px);
    return *px;
  }
  T* operator->() const throw()  // never throws
  {
    SHARED_ASSERT(NULL != px);
    return px;
  }
  T* get(void) const throw()  // never throws
  {
    // no assert, can return NULL
    return px;
  }

 private:
  /// @brief acquire/share the ownership of the px pointer, initializing the
  /// reference counter
  void acquire(T* p)  // may throw std::bad_alloc
  {
    pn.acquire(p);  // may throw std::bad_alloc
    px = p;  // here it is safe to acquire the ownership of the provided raw
             // pointer, where exception cannot be thrown any more
  }

  /// @brief release the ownership of the px pointer, destroying the object when
  /// appropriate
  void release(void) throw()  // never throws
  {
    pn.release(px);
    px = NULL;
  }

 private:
  // This allow pointer_cast functions to share the reference counter between
  // different ref types
  template <class U>
  friend class ref;

 private:
  T* px;         //!< Native pointer
  ref_count pn;  //!< Reference counter
};

// comparaison operators
template <class T, class U>
bool operator==(const ref<T>& l,
                const ref<U>& r) throw()  // never throws
{
  return (l.get() == r.get());
}
template <class T, class U>
bool operator!=(const ref<T>& l,
                const ref<U>& r) throw()  // never throws
{
  return (l.get() != r.get());
}
template <class T, class U>
bool operator<=(const ref<T>& l,
                const ref<U>& r) throw()  // never throws
{
  return (l.get() <= r.get());
}
template <class T, class U>
bool operator<(const ref<T>& l,
               const ref<U>& r) throw()  // never throws
{
  return (l.get() < r.get());
}
template <class T, class U>
bool operator>=(const ref<T>& l,
                const ref<U>& r) throw()  // never throws
{
  return (l.get() >= r.get());
}
template <class T, class U>
bool operator>(const ref<T>& l,
               const ref<U>& r) throw()  // never throws
{
  return (l.get() > r.get());
}

// static cast of ref
template <class T, class U>
ref<T> static_pointer_cast(const ref<U>& ptr)  // never throws
{
  return ref<T>(ptr, static_cast<typename ref<T>::element_type*>(ptr.get()));
}

// dynamic cast of ref
template <class T, class U>
ref<T> dynamic_pointer_cast(const ref<U>& ptr)  // never throws
{
  T* p = dynamic_cast<typename ref<T>::element_type*>(ptr.get());
  if (NULL != p) {
    return ref<T>(ptr, p);
  } else {
    return ref<T>();
  }
}

template <typename T, typename... Args>
ref<T> make_ref(Args&&... args) {
  return ref<T>(new T(forward<Args>(args)...));
}

#endif
