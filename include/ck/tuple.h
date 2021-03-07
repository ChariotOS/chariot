#pragma once


#define __NEED_size_t
#include <bits/alltypes.h>

namespace ck {
#if 1
  namespace detail {
    /*
     * because we need an implementation of `get_impl` in the main tuple class,
     * we have to create a `core` tuple class which is a parent to the main
     * tuple class. This lets us have methods on the tuple class like
     * `.get<N>()` or `.first()` which is a nice quality of life.
     */
    template <typename F, typename... Rest>
    struct tuple_core : public tuple_core<Rest...> {
      tuple_core(F first, Rest... rest) : tuple_core<Rest...>(rest...), first(first) {
      }
      F first;
    };

    template <typename F>
    struct tuple_core<F> {
      tuple_core(F first) : first(first) {
      }
      F first;
    };

    template <int index, typename F, typename... Rest>
    struct get_impl {
      static auto &value(tuple_core<F, Rest...> *t) {
        return get_impl<index - 1, Rest...>::value(t);
      }
    };

    template <typename F, typename... Rest>
    struct get_impl<0, F, Rest...> {
      static auto &value(tuple_core<F, Rest...> *t) {
        return t->first;
      }
    };
  }  // namespace detail

  template <typename F, typename... Rest>
  struct tuple : public ck::detail::tuple_core<F, Rest...> {
    tuple(F first, Rest... rest) : detail::tuple_core<F, Rest...>(first, rest...) {
    }

    template <int index>
    auto &get() {
      return ck::detail::get_impl<index, F, Rest...>::value(this);
    }

    // some common useful functions
    auto &first() {
      return get<0>();
    }
    auto &second() {
      return get<1>();
    }
  };
#else

  template <size_t...>
  struct __tuple_indices {};

  template <class _IdxType, _IdxType... _Values>
  struct __integer_sequence {
    template <template <class _OIdxType, _OIdxType...> class _ToIndexSeq, class _ToIndexType>
    using __convert = _ToIndexSeq<_ToIndexType, _Values...>;

    template <size_t _Sp>
    using __to_tuple_indices = __tuple_indices<(_Values + _Sp)...>;
  };

  namespace detail {

    template <typename _Tp, size_t... _Extra>
    struct __repeat;
    template <typename _Tp, _Tp... _Np, size_t... _Extra>
    struct __repeat<__integer_sequence<_Tp, _Np...>, _Extra...> {
      typedef __integer_sequence<_Tp, _Np..., sizeof...(_Np) + _Np..., 2 * sizeof...(_Np) + _Np...,
                                 3 * sizeof...(_Np) + _Np..., 4 * sizeof...(_Np) + _Np...,
                                 5 * sizeof...(_Np) + _Np..., 6 * sizeof...(_Np) + _Np...,
                                 7 * sizeof...(_Np) + _Np..., _Extra...>
          type;
    };

    template <size_t _Np>
    struct __parity;
    template <size_t _Np>
    struct __make : __parity<_Np % 8>::template __pmake<_Np> {};

    template <>
    struct __make<0> {
      typedef __integer_sequence<size_t> type;
    };
    template <>
    struct __make<1> {
      typedef __integer_sequence<size_t, 0> type;
    };
    template <>
    struct __make<2> {
      typedef __integer_sequence<size_t, 0, 1> type;
    };
    template <>
    struct __make<3> {
      typedef __integer_sequence<size_t, 0, 1, 2> type;
    };
    template <>
    struct __make<4> {
      typedef __integer_sequence<size_t, 0, 1, 2, 3> type;
    };
    template <>
    struct __make<5> {
      typedef __integer_sequence<size_t, 0, 1, 2, 3, 4> type;
    };
    template <>
    struct __make<6> {
      typedef __integer_sequence<size_t, 0, 1, 2, 3, 4, 5> type;
    };
    template <>
    struct __make<7> {
      typedef __integer_sequence<size_t, 0, 1, 2, 3, 4, 5, 6> type;
    };

    template <>
    struct __parity<0> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type> {};
    };
    template <>
    struct __parity<1> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type, _Np - 1> {};
    };
    template <>
    struct __parity<2> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type, _Np - 2, _Np - 1> {};
    };
    template <>
    struct __parity<3> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type, _Np - 3, _Np - 2, _Np - 1> {};
    };
    template <>
    struct __parity<4> {
      template <size_t _Np>
      struct __pmake
          : __repeat<typename __make<_Np / 8>::type, _Np - 4, _Np - 3, _Np - 2, _Np - 1> {};
    };
    template <>
    struct __parity<5> {
      template <size_t _Np>
      struct __pmake
          : __repeat<typename __make<_Np / 8>::type, _Np - 5, _Np - 4, _Np - 3, _Np - 2, _Np - 1> {
      };
    };
    template <>
    struct __parity<6> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type, _Np - 6, _Np - 5, _Np - 4, _Np - 3,
                                _Np - 2, _Np - 1> {};
    };
    template <>
    struct __parity<7> {
      template <size_t _Np>
      struct __pmake : __repeat<typename __make<_Np / 8>::type, _Np - 7, _Np - 6, _Np - 5, _Np - 4,
                                _Np - 3, _Np - 2, _Np - 1> {};
    };


    template <size_t _Ep, size_t _Sp>
    using __make_indices_imp =
        typename detail::__make<_Ep - _Sp>::type::template __to_tuple_indices<_Sp>;
    template <size_t _Ep, size_t _Sp = 0>
    struct make_tuple_indices {
      static_assert(_Sp <= _Ep, "__make_tuple_indices input error");
      typedef __make_indices_imp<_Ep, _Sp> type;
    };


  }  // namespace detail



  template <size_t _Ip, class _Hp>
  class __tuple_leaf {
   public:
    _Hp __value_;
  };


  template <class _Indx, class... _Tp>
  struct __tuple_impl;

  template <size_t... _Indx, class... _Tp>
  struct __tuple_impl<__tuple_indices<_Indx...>, _Tp...> : public __tuple_leaf<_Indx, _Tp>... {};

  template <class... _Tp>
  class tuple {
    typedef __tuple_impl<typename detail::make_tuple_indices<sizeof...(_Tp)>::type, _Tp...> _BaseT;
    _BaseT base;
  };
#endif

}  // namespace ck
