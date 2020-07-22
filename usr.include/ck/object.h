#pragma once


#include <ck/event.h>
#include <ck/intrusive_list.h>
#include <ck/ptr.h>

/*
 * macro for automatically implementing some basic ck::object methods
 */
#define CK_OBJECT(klass)                                             \
 public:                                                             \
  virtual const char* class_name() const override { return #klass; } \
  template <class... Args>                                           \
  static inline ck::ref<klass> create(Args&&... args) {              \
    return ck::make_ref<klass>(forward<Args>(args)...);              \
  }


#define CK_NONCOPYABLE(c) \
 private:                 \
  c(const c&) = delete;   \
  c& operator=(const c&) = delete;

#define CK_MAKE_NONMOVABLE(c) \
 private:                     \
  c(c&&) = delete;            \
  c& operator=(c&&) = delete;

namespace ck {


  // a ck::object is an interface that allows some cool generic operations on
  // any structure such as getting the class name or. They also exist as a large
  // list in memory to record all objects
  class object : public ck::refcounted<ck::object> {
   private:
    // intrusive list of all ck::objects that have been allocated
    ck::intrusive_list_node m_all_list;

   public:
    object(void);
    virtual ~object(void);
    /**
     * class_name - returns the name of the object's class as a string
     */
    virtual const char* class_name(void) const { return "ck::object"; };


    /*
     * event - notify the ck::object of some external event. Returns if it has
     * consumed the event.
     */
    virtual bool event(const ck::event&) { return false; }

		/**
		 * return a list of all the ck::objects alive
		 */
    ck::intrusive_list<ck::object, &ck::object::m_all_list>& all_objects();
  };
}  // namespace ck
