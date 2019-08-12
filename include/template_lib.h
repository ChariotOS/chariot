#ifndef __TEMPLATE_LIB_H__
#define __TEMPLATE_LIB_H__

typedef decltype(nullptr) nullptr_t;


template <typename T>
T&& move(T& arg) {
  return static_cast<T&&>(arg);
}

template <typename T>
struct Identity {
  typedef T Type;
};

template <class T>
constexpr T&& forward(typename Identity<T>::Type& param) {
  return static_cast<T&&>(param);
}

template <typename T, typename U>
T exchange(T& a, U&& b) {
  T tmp = move(a);
  a = move(b);
  return tmp;
}

template <typename T, typename U>
void swap(T& a, U& b) {
  U tmp = move((U&)a);
  a = (T &&) move(b);
  b = move(tmp);
}

template <bool B, class T = void>
struct EnableIf {};

template <class T>
struct EnableIf<true, T> {
  typedef T Type;
};

template <class T>
struct RemoveConst {
  typedef T Type;
};
template <class T>
struct RemoveConst<const T> {
  typedef T Type;
};
template <class T>
struct RemoveVolatile {
  typedef T Type;
};
template <class T>
struct RemoveVolatile<volatile T> {
  typedef T Type;
};
template <class T>
struct RemoveCV {
  typedef typename RemoveVolatile<typename RemoveConst<T>::Type>::Type Type;
};

template <class T, T v>
struct IntegralConstant {
  static constexpr T value = v;
  typedef T ValueType;
  typedef IntegralConstant Type;
  constexpr operator ValueType() const { return value; }
  constexpr ValueType operator()() const { return value; }
};

typedef IntegralConstant<bool, false> FalseType;
typedef IntegralConstant<bool, true> TrueType;

template <class T>
struct __IsPointerHelper : FalseType {};

template <class T>
struct __IsPointerHelper<T*> : TrueType {};

template <class T>
struct IsPointer : __IsPointerHelper<typename RemoveCV<T>::Type> {};

template <class>
struct IsFunction : FalseType {};

template <class Ret, class... Args>
struct IsFunction<Ret(Args...)> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...)> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) volatile> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) volatile> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const volatile> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const volatile> : TrueType {};

template <class Ret, class... Args>
struct IsFunction<Ret(Args...)&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...)&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) volatile&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) volatile&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const volatile&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const volatile&> : TrueType {};

template <class Ret, class... Args>
struct IsFunction<Ret(Args...) &&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) &&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const&&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const&&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) volatile&&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) volatile&&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args...) const volatile&&> : TrueType {};
template <class Ret, class... Args>
struct IsFunction<Ret(Args..., ...) const volatile&&> : TrueType {};

template <class T>
struct IsRvalueReference : FalseType {};
template <class T>
struct IsRvalueReference<T&&> : TrueType {};

template <class T>
struct RemovePointer {
  typedef T Type;
};
template <class T>
struct RemovePointer<T*> {
  typedef T Type;
};
template <class T>
struct RemovePointer<T* const> {
  typedef T Type;
};
template <class T>
struct RemovePointer<T* volatile> {
  typedef T Type;
};
template <class T>
struct RemovePointer<T* const volatile> {
  typedef T Type;
};

template <typename T, typename U>
struct IsSame {
  enum { value = 0 };
};

template <typename T>
struct IsSame<T, T> {
  enum { value = 1 };
};

#endif
