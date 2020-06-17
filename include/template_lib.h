#pragma once

#ifndef __TEMPLATE_LIB_H__
#define __TEMPLATE_LIB_H__


#ifdef USERLAND

#else
#include <printk.h>
#endif


typedef decltype(nullptr) nullptr_t;

template <typename T>
struct GenericTraits {
  static constexpr bool is_trivial() { return false; }
  static bool equals(const T& a, const T& b) { return a == b; }
};

template <typename T>
struct Traits : public GenericTraits<T> {};

template <typename T>
struct IntegerTraits : public GenericTraits<T> {
  static constexpr bool is_trivial() { return true; }
  static unsigned long hash(T c) { return c; }
  static int cmp(T a, T b) { return a - b; }
};

#define DEF_INT_TRAITS(T) \
  template <>             \
  struct Traits<T> : public IntegerTraits<T> {};

DEF_INT_TRAITS(unsigned char);
DEF_INT_TRAITS(char);

DEF_INT_TRAITS(short);
DEF_INT_TRAITS(unsigned short);

DEF_INT_TRAITS(int);
DEF_INT_TRAITS(unsigned int);

DEF_INT_TRAITS(long);
DEF_INT_TRAITS(unsigned long);

template <typename T>
struct Traits<T*> {
  static unsigned hash(const T* p) {
    return (unsigned long long)p;  // int_hash((unsigned)(__PTRDIFF_TYPE__)p);
  }
  static constexpr bool is_trivial() { return true; }
  static void dump(const T* p) { printk("%p", p); }
  static bool equals(const T* a, const T* b) { return a == b; }
};

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
