#ifndef __FUNC_H__
#define __FUNC_H__

template<typename T>
struct Identity {
    typedef T Type;
};

template <class T>
constexpr T&& forward(typename Identity<T>::Type& param) {
  return static_cast<T&&>(param);
}

template <typename>
class func;

template <typename Ret, typename... Args>
class func<Ret(Args...)> {
 public:
  func() = default;

  Ret operator()(Args... in) const {
    /*
    ASSERT(m_callable_wrapper);
    return m_callable_wrapper->call(forward<Args>(in)...);
    */
  }
};

#endif
