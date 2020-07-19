#pragma once

namespace ck {
  namespace detail {
    /*
     * because we need an implementation of `get_impl` in the main tuple class,
     * we have to create a `core` tuple class which is a parent to the main
     * tuple class. This lets us have methods on the tuple class like
     * `.get<N>()` or `.first()` which is a nice quality of life.
     */
    template <typename F, typename... Rest>
    struct tuple_core : public tuple_core<Rest...> {
      tuple_core(F first, Rest... rest)
          : tuple_core<Rest...>(rest...), first(first) {}
      F first;
    };

    template <typename F>
    struct tuple_core<F> {
      tuple_core(F first) : first(first) {}
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
    tuple(F first, Rest... rest)
        : detail::tuple_core<F, Rest...>(first, rest...) {}

    template <int index>
    auto &get() {
      return ck::detail::get_impl<index, F, Rest...>::value(this);
    }

		// some common useful functions
    auto &first() { return get<0>(); }
    auto &second() { return get<1>(); }
  };
}  // namespace ck
