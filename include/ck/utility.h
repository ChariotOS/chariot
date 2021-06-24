#pragma once

#include <unistd.h>


/// much of this is stolen from gcc's libckc++

namespace ck {




  /// integral_constant
  template <typename _Tp, _Tp __v>
  struct integral_constant {
    static constexpr _Tp value = __v;
    typedef _Tp value_type;
    typedef integral_constant<_Tp, __v> type;
    constexpr operator value_type() const noexcept { return value; }
#if __cplusplus > 201103L

#define __cpp_lib_integral_constant_callable 201304

    constexpr value_type operator()() const noexcept { return value; }
#endif
  };

  template <typename _Tp, _Tp __v>
  constexpr _Tp integral_constant<_Tp, __v>::value;

  /// The type used as a compile-time boolean with true value.
  typedef integral_constant<bool, true> true_type;

  /// The type used as a compile-time boolean with false value.
  typedef integral_constant<bool, false> false_type;

  template <bool __v>
  using __bool_constant = integral_constant<bool, __v>;




  // Meta programming helper types.

  template <bool, typename, typename>
  struct conditional;

  template <typename _Type>
  struct __type_identity {
    using type = _Type;
  };

  template <typename _Tp>
  using __type_identity_t = typename __type_identity<_Tp>::type;

  /// or
  template <typename...>
  struct __or_;

  template <>
  struct __or_<> : public false_type {};

  template <typename _B1>
  struct __or_<_B1> : public _B1 {};

  template <typename _B1, typename _B2>
  struct __or_<_B1, _B2> : public conditional<_B1::value, _B1, _B2>::type {};

  template <typename _B1, typename _B2, typename _B3, typename... _Bn>
  struct __or_<_B1, _B2, _B3, _Bn...>
      : public conditional<_B1::value, _B1, __or_<_B2, _B3, _Bn...>>::type {};



  /// and
  template <typename...>
  struct __and_;

  template <>
  struct __and_<> : public true_type {};

  template <typename _B1>
  struct __and_<_B1> : public _B1 {};

  template <typename _B1, typename _B2>
  struct __and_<_B1, _B2> : public conditional<_B1::value, _B2, _B1>::type {};

  template <typename _B1, typename _B2, typename _B3, typename... _Bn>
  struct __and_<_B1, _B2, _B3, _Bn...>
      : public conditional<_B1::value, __and_<_B2, _B3, _Bn...>, _B1>::type {};

  template <typename _Pp>
  struct __not_ : public __bool_constant<!bool(_Pp::value)> {};




  // Forward declarations
  template <typename>
  struct is_reference;
  template <typename>
  struct is_function;
  template <typename>
  struct is_void;
  template <typename>
  struct __is_array_unknown_bounds;


  // Helper functions that return false_type for incomplete classes,
  // incomplete unions and arrays of known bound from those.
  template <typename _Tp, size_t = sizeof(_Tp)>
  constexpr true_type __is_complete_or_unbounded(__type_identity<_Tp>) {
    return {};
  }

  template <typename _TypeIdentity, typename _NestedType = typename _TypeIdentity::type>
  constexpr typename __or_<is_reference<_NestedType>, is_function<_NestedType>,
                           is_void<_NestedType>, __is_array_unknown_bounds<_NestedType>>::type
  __is_complete_or_unbounded(_TypeIdentity) {
    return {};
  }



  // For several sfinae-friendly trait implementations we transport both the
  // result information (as the member type) and the failure information (no
  // member type). This is very similar to ck::enable_if, but we cannot use
  // them, because we need to derive from them as an implementation detail.

  template <typename _Tp>
  struct __success_type {
    typedef _Tp type;
  };

  struct __failure_type {};

  template <typename>
  struct remove_cv;

  // __remove_cv_t (ck::remove_cv_t for C++11).
  template <typename _Tp>
  using __remove_cv_t = typename remove_cv<_Tp>::type;

  template <typename>
  struct is_const;

  // Primary type categories.

  template <typename>
  struct __is_void_helper : public false_type {};




  template <>
  struct __is_void_helper<void> : public true_type {};

  /// is_void
  template <typename _Tp>
  struct is_void : public __is_void_helper<__remove_cv_t<_Tp>>::type {};

  template <typename>
  struct __is_integral_helper : public false_type {};

  template <>
  struct __is_integral_helper<bool> : public true_type {};

  template <>
  struct __is_integral_helper<char> : public true_type {};

  template <>
  struct __is_integral_helper<signed char> : public true_type {};

  template <>
  struct __is_integral_helper<unsigned char> : public true_type {};



  template <>
  struct __is_integral_helper<char16_t> : public true_type {};

  template <>
  struct __is_integral_helper<char32_t> : public true_type {};

  template <>
  struct __is_integral_helper<short> : public true_type {};

  template <>
  struct __is_integral_helper<unsigned short> : public true_type {};

  template <>
  struct __is_integral_helper<int> : public true_type {};

  template <>
  struct __is_integral_helper<unsigned int> : public true_type {};

  template <>
  struct __is_integral_helper<long> : public true_type {};

  template <>
  struct __is_integral_helper<unsigned long> : public true_type {};

  template <>
  struct __is_integral_helper<long long> : public true_type {};

  template <>
  struct __is_integral_helper<unsigned long long> : public true_type {};



  /// is_integral
  template <typename _Tp>
  struct is_integral : public __is_integral_helper<__remove_cv_t<_Tp>>::type {};

  template <typename>
  struct __is_floating_point_helper : public false_type {};

  template <>
  struct __is_floating_point_helper<float> : public true_type {};

  template <>
  struct __is_floating_point_helper<double> : public true_type {};

  template <>
  struct __is_floating_point_helper<long double> : public true_type {};




  /// is_floating_point
  template <typename _Tp>
  struct is_floating_point : public __is_floating_point_helper<__remove_cv_t<_Tp>>::type {};

  /// is_array
  template <typename>
  struct is_array : public false_type {};

  template <typename _Tp, size_t _Size>
  struct is_array<_Tp[_Size]> : public true_type {};

  template <typename _Tp>
  struct is_array<_Tp[]> : public true_type {};

  template <typename>
  struct __is_pointer_helper : public false_type {};

  template <typename _Tp>
  struct __is_pointer_helper<_Tp*> : public true_type {};

  /// is_pointer
  template <typename _Tp>
  struct is_pointer : public __is_pointer_helper<__remove_cv_t<_Tp>>::type {};

  /// is_lvalue_reference
  template <typename>
  struct is_lvalue_reference : public false_type {};

  template <typename _Tp>
  struct is_lvalue_reference<_Tp&> : public true_type {};

  /// is_rvalue_reference
  template <typename>
  struct is_rvalue_reference : public false_type {};

  template <typename _Tp>
  struct is_rvalue_reference<_Tp&&> : public true_type {};

  template <typename>
  struct __is_member_object_pointer_helper : public false_type {};

  template <typename _Tp, typename _Cp>
  struct __is_member_object_pointer_helper<_Tp _Cp::*> : public __not_<is_function<_Tp>>::type {};

  /// is_member_object_pointer
  template <typename _Tp>
  struct is_member_object_pointer
      : public __is_member_object_pointer_helper<__remove_cv_t<_Tp>>::type {};

  template <typename>
  struct __is_member_function_pointer_helper : public false_type {};

  template <typename _Tp, typename _Cp>
  struct __is_member_function_pointer_helper<_Tp _Cp::*> : public is_function<_Tp>::type {};




  /// is_member_function_pointer
  template <typename _Tp>
  struct is_member_function_pointer
      : public __is_member_function_pointer_helper<__remove_cv_t<_Tp>>::type {};

  /// is_enum
  template <typename _Tp>
  struct is_enum : public integral_constant<bool, __is_enum(_Tp)> {};

  /// is_union
  template <typename _Tp>
  struct is_union : public integral_constant<bool, __is_union(_Tp)> {};

  /// is_class
  template <typename _Tp>
  struct is_class : public integral_constant<bool, __is_class(_Tp)> {};

  /// is_function
  template <typename _Tp>
  struct is_function : public __bool_constant<!is_const<const _Tp>::value> {};

  template <typename _Tp>
  struct is_function<_Tp&> : public false_type {};

  template <typename _Tp>
  struct is_function<_Tp&&> : public false_type {};



  template <typename>
  struct __is_null_pointer_helper : public false_type {};

  template <>
  struct __is_null_pointer_helper<decltype(nullptr)> : public true_type {};

  /// is_null_pointer (LWG 2247).
  template <typename _Tp>
  struct is_null_pointer : public __is_null_pointer_helper<__remove_cv_t<_Tp>>::type {};

  // Composite type categories.

  /// is_reference
  template <typename _Tp>
  struct is_reference : public __or_<is_lvalue_reference<_Tp>, is_rvalue_reference<_Tp>>::type {};

  /// is_arithmetic
  template <typename _Tp>
  struct is_arithmetic : public __or_<is_integral<_Tp>, is_floating_point<_Tp>>::type {};

  /// is_fundamental
  template <typename _Tp>
  struct is_fundamental
      : public __or_<is_arithmetic<_Tp>, is_void<_Tp>, is_null_pointer<_Tp>>::type {};

  /// is_object
  template <typename _Tp>
  struct is_object : public __not_<__or_<is_function<_Tp>, is_reference<_Tp>, is_void<_Tp>>>::type {
  };

  template <typename>
  struct is_member_pointer;

  /// is_scalar
  template <typename _Tp>
  struct is_scalar : public __or_<is_arithmetic<_Tp>, is_enum<_Tp>, is_pointer<_Tp>,
                                  is_member_pointer<_Tp>, is_null_pointer<_Tp>>::type {};

  /// is_compound
  template <typename _Tp>
  struct is_compound : public __not_<is_fundamental<_Tp>>::type {};

  template <typename _Tp>
  struct __is_member_pointer_helper : public false_type {};

  template <typename _Tp, typename _Cp>
  struct __is_member_pointer_helper<_Tp _Cp::*> : public true_type {};

  /// is_member_pointer
  template <typename _Tp>
  struct is_member_pointer : public __is_member_pointer_helper<__remove_cv_t<_Tp>>::type {};

  template <typename, typename>
  struct is_same;

  template <typename _Tp, typename... _Types>
  using __is_one_of = __or_<is_same<_Tp, _Types>...>;


  // __void_t (ck::void_t for C++11)
  template <typename...>
  using __void_t = void;

  // Utility to detect referenceable types ([defns.referenceable]).

  template <typename _Tp, typename = void>
  struct __is_referenceable : public false_type {};

  template <typename _Tp>
  struct __is_referenceable<_Tp, __void_t<_Tp&>> : public true_type {};

  // Type properties.

  /// is_const
  template <typename>
  struct is_const : public false_type {};

  template <typename _Tp>
  struct is_const<_Tp const> : public true_type {};

  /// is_volatile
  template <typename>
  struct is_volatile : public false_type {};

  template <typename _Tp>
  struct is_volatile<_Tp volatile> : public true_type {};

  /// is_trivial
  template <typename _Tp>
  struct is_trivial : public integral_constant<bool, __is_trivial(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  // is_trivially_copyable
  template <typename _Tp>
  struct is_trivially_copyable : public integral_constant<bool, __is_trivially_copyable(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_standard_layout
  template <typename _Tp>
  struct is_standard_layout : public integral_constant<bool, __is_standard_layout(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };


  /// is_literal_type
  template <typename _Tp>
  struct is_literal_type : public integral_constant<bool, __is_literal_type(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };



  /// is_abstract
  template <typename _Tp>
  struct is_abstract : public integral_constant<bool, __is_abstract(_Tp)> {};

  template <typename _Tp, bool = is_arithmetic<_Tp>::value>
  struct __is_signed_helper : public false_type {};

  template <typename _Tp>
  struct __is_signed_helper<_Tp, true> : public integral_constant<bool, _Tp(-1) < _Tp(0)> {};

  /// is_signed
  template <typename _Tp>
  struct is_signed : public __is_signed_helper<_Tp>::type {};

  /// is_unsigned
  template <typename _Tp>
  struct is_unsigned : public __and_<is_arithmetic<_Tp>, __not_<is_signed<_Tp>>> {};


  /// is_empty
  template <typename _Tp>
  struct is_empty : public integral_constant<bool, __is_empty(_Tp)> {};

  /// is_polymorphic
  template <typename _Tp>
  struct is_polymorphic : public integral_constant<bool, __is_polymorphic(_Tp)> {};




  // Destructible and constructible type properties.

  /**
   *  @brief  Utility to simplify expressions used in unevaluated operands
   *  @ingroup utilities
   */

  template <typename _Tp, typename _Up = _Tp&&>
  _Up __declval(int);

  template <typename _Tp>
  _Tp __declval(long);

  template <typename _Tp>
  auto declval() noexcept -> decltype(__declval<_Tp>(0));

  template <typename, unsigned = 0>
  struct extent;

  template <typename>
  struct remove_all_extents;

  template <typename _Tp>
  struct __is_array_known_bounds : public integral_constant<bool, (extent<_Tp>::value > 0)> {};

  template <typename _Tp>
  struct __is_array_unknown_bounds : public __and_<is_array<_Tp>, __not_<extent<_Tp>>> {};



  // In N3290 is_destructible does not say anything about function
  // types and abstract types, see LWG 2049. This implementation
  // describes function types as non-destructible and all complete
  // object types as destructible, iff the explicit destructor
  // call expression is wellformed.
  struct __do_is_destructible_impl {
    template <typename _Tp, typename = decltype(declval<_Tp&>().~_Tp())>
    static true_type __test(int);

    template <typename>
    static false_type __test(...);
  };

  template <typename _Tp>
  struct __is_destructible_impl : public __do_is_destructible_impl {
    typedef decltype(__test<_Tp>(0)) type;
  };

  template <typename _Tp,
            bool = __or_<is_void<_Tp>, __is_array_unknown_bounds<_Tp>, is_function<_Tp>>::value,
            bool = __or_<is_reference<_Tp>, is_scalar<_Tp>>::value>
  struct __is_destructible_safe;

  template <typename _Tp>
  struct __is_destructible_safe<_Tp, false, false>
      : public __is_destructible_impl<typename remove_all_extents<_Tp>::type>::type {};

  template <typename _Tp>
  struct __is_destructible_safe<_Tp, true, false> : public false_type {};

  template <typename _Tp>
  struct __is_destructible_safe<_Tp, false, true> : public true_type {};

  /// is_destructible
  template <typename _Tp>
  struct is_destructible : public __is_destructible_safe<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };



  // is_nothrow_destructible requires that is_destructible is
  // satisfied as well.  We realize that by mimicing the
  // implementation of is_destructible but refer to noexcept(expr)
  // instead of decltype(expr).
  struct __do_is_nt_destructible_impl {
    template <typename _Tp>
    static __bool_constant<noexcept(declval<_Tp&>().~_Tp())> __test(int);

    template <typename>
    static false_type __test(...);
  };

  template <typename _Tp>
  struct __is_nt_destructible_impl : public __do_is_nt_destructible_impl {
    typedef decltype(__test<_Tp>(0)) type;
  };

  template <typename _Tp,
            bool = __or_<is_void<_Tp>, __is_array_unknown_bounds<_Tp>, is_function<_Tp>>::value,
            bool = __or_<is_reference<_Tp>, is_scalar<_Tp>>::value>
  struct __is_nt_destructible_safe;

  template <typename _Tp>
  struct __is_nt_destructible_safe<_Tp, false, false>
      : public __is_nt_destructible_impl<typename remove_all_extents<_Tp>::type>::type {};

  template <typename _Tp>
  struct __is_nt_destructible_safe<_Tp, true, false> : public false_type {};

  template <typename _Tp>
  struct __is_nt_destructible_safe<_Tp, false, true> : public true_type {};

  /// is_nothrow_destructible
  template <typename _Tp>
  struct is_nothrow_destructible : public __is_nt_destructible_safe<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, typename... _Args>
  struct __is_constructible_impl : public __bool_constant<__is_constructible(_Tp, _Args...)> {};

  /// is_constructible
  template <typename _Tp, typename... _Args>
  struct is_constructible : public __is_constructible_impl<_Tp, _Args...> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_default_constructible
  template <typename _Tp>
  struct is_default_constructible : public __is_constructible_impl<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_copy_constructible_impl;

  template <typename _Tp>
  struct __is_copy_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_copy_constructible_impl<_Tp, true> : public __is_constructible_impl<_Tp, const _Tp&> {
  };

  /// is_copy_constructible
  template <typename _Tp>
  struct is_copy_constructible : public __is_copy_constructible_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_move_constructible_impl;

  template <typename _Tp>
  struct __is_move_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_move_constructible_impl<_Tp, true> : public __is_constructible_impl<_Tp, _Tp&&> {};

  /// is_move_constructible
  template <typename _Tp>
  struct is_move_constructible : public __is_move_constructible_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, typename... _Args>
  using __is_nothrow_constructible_impl =
      __bool_constant<__is_nothrow_constructible(_Tp, _Args...)>;

  /// is_nothrow_constructible
  template <typename _Tp, typename... _Args>
  struct is_nothrow_constructible : public __is_nothrow_constructible_impl<_Tp, _Args...>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_nothrow_default_constructible
  template <typename _Tp>
  struct is_nothrow_default_constructible
      : public __bool_constant<__is_nothrow_constructible(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };


  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_nothrow_copy_constructible_impl;

  template <typename _Tp>
  struct __is_nothrow_copy_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_nothrow_copy_constructible_impl<_Tp, true>
      : public __is_nothrow_constructible_impl<_Tp, const _Tp&> {};

  /// is_nothrow_copy_constructible
  template <typename _Tp>
  struct is_nothrow_copy_constructible : public __is_nothrow_copy_constructible_impl<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_nothrow_move_constructible_impl;

  template <typename _Tp>
  struct __is_nothrow_move_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_nothrow_move_constructible_impl<_Tp, true>
      : public __is_nothrow_constructible_impl<_Tp, _Tp&&> {};

  /// is_nothrow_move_constructible
  template <typename _Tp>
  struct is_nothrow_move_constructible : public __is_nothrow_move_constructible_impl<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_assignable
  template <typename _Tp, typename _Up>
  struct is_assignable : public __bool_constant<__is_assignable(_Tp, _Up)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_copy_assignable_impl;

  template <typename _Tp>
  struct __is_copy_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_copy_assignable_impl<_Tp, true>
      : public __bool_constant<__is_assignable(_Tp&, const _Tp&)> {};

  /// is_copy_assignable
  template <typename _Tp>
  struct is_copy_assignable : public __is_copy_assignable_impl<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_move_assignable_impl;

  template <typename _Tp>
  struct __is_move_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_move_assignable_impl<_Tp, true>
      : public __bool_constant<__is_assignable(_Tp&, _Tp&&)> {};

  /// is_move_assignable
  template <typename _Tp>
  struct is_move_assignable : public __is_move_assignable_impl<_Tp>::type {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, typename _Up>
  using __is_nothrow_assignable_impl = __bool_constant<__is_nothrow_assignable(_Tp, _Up)>;

  /// is_nothrow_assignable
  template <typename _Tp, typename _Up>
  struct is_nothrow_assignable : public __is_nothrow_assignable_impl<_Tp, _Up> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_nt_copy_assignable_impl;

  template <typename _Tp>
  struct __is_nt_copy_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_nt_copy_assignable_impl<_Tp, true>
      : public __is_nothrow_assignable_impl<_Tp&, const _Tp&> {};

  /// is_nothrow_copy_assignable
  template <typename _Tp>
  struct is_nothrow_copy_assignable : public __is_nt_copy_assignable_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_nt_move_assignable_impl;

  template <typename _Tp>
  struct __is_nt_move_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_nt_move_assignable_impl<_Tp, true>
      : public __is_nothrow_assignable_impl<_Tp&, _Tp&&> {};

  /// is_nothrow_move_assignable
  template <typename _Tp>
  struct is_nothrow_move_assignable : public __is_nt_move_assignable_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_trivially_constructible
  template <typename _Tp, typename... _Args>
  struct is_trivially_constructible
      : public __bool_constant<__is_trivially_constructible(_Tp, _Args...)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_trivially_default_constructible
  template <typename _Tp>
  struct is_trivially_default_constructible
      : public __bool_constant<__is_trivially_constructible(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  struct __do_is_implicitly_default_constructible_impl {
    template <typename _Tp>
    static void __helper(const _Tp&);

    template <typename _Tp>
    static true_type __test(const _Tp&, decltype(__helper<const _Tp&>({}))* = 0);

    static false_type __test(...);
  };

  template <typename _Tp>
  struct __is_implicitly_default_constructible_impl
      : public __do_is_implicitly_default_constructible_impl {
    typedef decltype(__test(declval<_Tp>())) type;
  };

  template <typename _Tp>
  struct __is_implicitly_default_constructible_safe
      : public __is_implicitly_default_constructible_impl<_Tp>::type {};

  template <typename _Tp>
  struct __is_implicitly_default_constructible
      : public __and_<__is_constructible_impl<_Tp>,
                      __is_implicitly_default_constructible_safe<_Tp>> {};

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_trivially_copy_constructible_impl;

  template <typename _Tp>
  struct __is_trivially_copy_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_trivially_copy_constructible_impl<_Tp, true>
      : public __and_<__is_copy_constructible_impl<_Tp>,
                      integral_constant<bool, __is_trivially_constructible(_Tp, const _Tp&)>> {};

  /// is_trivially_copy_constructible
  template <typename _Tp>
  struct is_trivially_copy_constructible : public __is_trivially_copy_constructible_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_trivially_move_constructible_impl;

  template <typename _Tp>
  struct __is_trivially_move_constructible_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_trivially_move_constructible_impl<_Tp, true>
      : public __and_<__is_move_constructible_impl<_Tp>,
                      integral_constant<bool, __is_trivially_constructible(_Tp, _Tp&&)>> {};

  /// is_trivially_move_constructible
  template <typename _Tp>
  struct is_trivially_move_constructible : public __is_trivially_move_constructible_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_trivially_assignable
  template <typename _Tp, typename _Up>
  struct is_trivially_assignable : public __bool_constant<__is_trivially_assignable(_Tp, _Up)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_trivially_copy_assignable_impl;

  template <typename _Tp>
  struct __is_trivially_copy_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_trivially_copy_assignable_impl<_Tp, true>
      : public __bool_constant<__is_trivially_assignable(_Tp&, const _Tp&)> {};

  /// is_trivially_copy_assignable
  template <typename _Tp>
  struct is_trivially_copy_assignable : public __is_trivially_copy_assignable_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  template <typename _Tp, bool = __is_referenceable<_Tp>::value>
  struct __is_trivially_move_assignable_impl;

  template <typename _Tp>
  struct __is_trivially_move_assignable_impl<_Tp, false> : public false_type {};

  template <typename _Tp>
  struct __is_trivially_move_assignable_impl<_Tp, true>
      : public __bool_constant<__is_trivially_assignable(_Tp&, _Tp&&)> {};

  /// is_trivially_move_assignable
  template <typename _Tp>
  struct is_trivially_move_assignable : public __is_trivially_move_assignable_impl<_Tp> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// is_trivially_destructible
  template <typename _Tp>
  struct is_trivially_destructible
      : public __and_<__is_destructible_safe<_Tp>, __bool_constant<__has_trivial_destructor(_Tp)>> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };


  /// has_virtual_destructor
  template <typename _Tp>
  struct has_virtual_destructor : public integral_constant<bool, __has_virtual_destructor(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };




  // type property queries.

  /// alignment_of
  template <typename _Tp>
  struct alignment_of : public integral_constant<size_t, alignof(_Tp)> {
    static_assert(ck::__is_complete_or_unbounded(__type_identity<_Tp>{}),
                  "template argument must be a complete class or an unbounded array");
  };

  /// rank
  template <typename>
  struct rank : public integral_constant<size_t, 0> {};

  template <typename _Tp, size_t _Size>
  struct rank<_Tp[_Size]> : public integral_constant<size_t, 1 + rank<_Tp>::value> {};

  template <typename _Tp>
  struct rank<_Tp[]> : public integral_constant<size_t, 1 + rank<_Tp>::value> {};

  /// extent
  template <typename, unsigned _Uint>
  struct extent : public integral_constant<size_t, 0> {};

  template <typename _Tp, unsigned _Uint, size_t _Size>
  struct extent<_Tp[_Size], _Uint>
      : public integral_constant<size_t, _Uint == 0 ? _Size : extent<_Tp, _Uint - 1>::value> {};

  template <typename _Tp, unsigned _Uint>
  struct extent<_Tp[], _Uint>
      : public integral_constant<size_t, _Uint == 0 ? 0 : extent<_Tp, _Uint - 1>::value> {};



  // Type relations.

  /// is_same
  template <typename _Tp, typename _Up>
  struct is_same
#ifdef _GLIBCXX_HAVE_BUILTIN_IS_SAME
      : public integral_constant<bool, __is_same(_Tp, _Up)>
#else
      : public false_type
#endif
  {
  };

#ifndef _GLIBCXX_HAVE_BUILTIN_IS_SAME
  template <typename _Tp>
  struct is_same<_Tp, _Tp> : public true_type {};
#endif

  /// is_base_of
  template <typename _Base, typename _Derived>
  struct is_base_of : public integral_constant<bool, __is_base_of(_Base, _Derived)> {};

  template <typename _From, typename _To,
            bool = __or_<is_void<_From>, is_function<_To>, is_array<_To>>::value>
  struct __is_convertible_helper {
    typedef typename is_void<_To>::type type;
  };


  // I got to line 1349 of
  // https://github.com/gcc-mirror/gcc/blob/master/libstdc%2B%2B-v3/include/std/type_traits. feel
  // free to continue later.


  // Reference transformations.

  /// remove_reference
  template <typename _Tp>
  struct remove_reference {
    typedef _Tp type;
  };

  template <typename _Tp>
  struct remove_reference<_Tp&> {
    typedef _Tp type;
  };

  template <typename _Tp>
  struct remove_reference<_Tp&&> {
    typedef _Tp type;
  };



  template <typename _Tp>
  inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<_Tp>::value;


  /**
   *  @brief  Forward an lvalue.
   *  @return The parameter cast to the specified type.
   *
   *  This function is used to implement "perfect forwarding".
   */
  template <typename T>
  constexpr T&& forward(typename ck::remove_reference<T>::type& value) noexcept {
    return static_cast<T&&>(value);
  }

  /**
   *  @brief  Forward an rvalue.
   *  @return The parameter cast to the specified type.
   *
   *  This function is used to implement "perfect forwarding".
   */
  template <typename T>
  constexpr T&& forward(typename ck::remove_reference<T>::type&& value) noexcept {
    static_assert(!ck::is_lvalue_reference<T>::value,
                  "template argument"
                  " substituting T must not be an lvalue reference type");
    return static_cast<T&&>(value);
  }


}  // namespace ck