
// think-cell public library
//
// Copyright (C) 2016-2023 think-cell Software GmbH
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

#pragma once

#include "assert_defs.h"

////////////////////////////////
// functor equivalents for operators, free functions and member functions

#include "move.h"
#include "return_decltype.h"
#include "tag_type.h"

#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/control/if.hpp>

#include <cmath>
#include <cstdlib>
#include <algorithm>

#define DEFINE_FN2( func, name ) \
	struct [[nodiscard]] name { \
		template< typename... Args > \
		constexpr auto operator()( Args&& ... args) /*no &*/ const \
			return_decltype_xvalue_by_ref_MAYTHROW( func(tc_move_if_owned(args)...) ) \
		using is_transparent=void; \
	};

#define DEFINE_MEM_FN_BODY_( ... ) \
	template< typename O, typename... __A > \
	constexpr auto operator()( O&& o, __A&& ... __a ) const \
		return_decltype_xvalue_by_ref_MAYTHROW( tc_move_if_owned(o) __VA_ARGS__ ( tc_move_if_owned(__a)... ) )

// std::mem_fn (C++11 standard 20.8.2) knows the type it can apply its member function pointer to and
// dereferences via operator* until it reaches that something of that type. We cannot do that because
// we have no type. Merely checking for the presence of the member function name is dangerous because
// changes in otherwise unrelated types (the iterator and its pointee) would influence the lookup. 
// Instead we distinguish between shallow member access, implemented in dot_mem_fn, and deep 
// member access via operator->, with the small addition that if operator-> does not exist, we use the
// the dot operator, in the spirit of dereferencing as much as possible.

#define DEFINE_MEM_FN_AUTO_BODY_( ... ) \
	template< typename O, typename... __A, std::enable_if_t<tc::has_operator_arrow<O>::value>* = nullptr > \
	constexpr auto operator()( O&& o, __A&& ... __a ) const \
		return_decltype_xvalue_by_ref_MAYTHROW( tc_move_if_owned(o)-> __VA_ARGS__ ( tc_move_if_owned(__a)... ) ) \
	template< typename O, typename... __A, std::enable_if_t<!tc::has_operator_arrow<O>::value>* = nullptr > \
	constexpr auto operator()( O&& o, __A&& ... __a ) const \
		return_decltype_xvalue_by_ref_MAYTHROW( tc_move_if_owned(o). __VA_ARGS__ ( tc_move_if_owned(__a)... ) )

// When a functor must be declared in class-scope, e.g., to access a protected member function,
// you should use DEFINE_MEM_FN instead of tc_define_fn. 
// tc_define_fn always has to define a function named "void func(define_fn_dummy_t)" which may
// shadow inherited members named func.
#define DEFINE_MEM_FN( func ) \
	struct [[nodiscard]] dot_member_ ## func { \
		template< typename O > constexpr auto operator()( O & o ) const return_decltype_noexcept( o.func ) \
		template< typename O > constexpr auto operator()( O const& o ) const return_decltype_noexcept( o.func ) \
		template< typename O, std::enable_if_t<!std::is_reference<O>::value>* = nullptr > \
		constexpr auto operator()( O&& o ) const return_decltype_xvalue_by_ref_noexcept( tc_move(o).func ) \
	}; \
	tc::constant<true> returns_reference_to_argument(dot_member_ ## func); /*mark as returning reference to argument*/ \
	struct [[nodiscard]] dot_mem_fn_ ## func { \
		DEFINE_MEM_FN_BODY_( .func ) \
	}; \
	struct [[nodiscard]] mem_fn_ ## func { \
		DEFINE_MEM_FN_AUTO_BODY_( func ) \
	};

#define tc_define_fn( func ) \
	static void func(tc::define_fn_dummy_t) noexcept = delete; \
	DEFINE_FN2( func, fn_ ## func ) \
	DEFINE_MEM_FN( func )

#define MEM_FN_TEMPLATE_DECLARATION(r, data, i, elem) BOOST_PP_COMMA_IF(i) elem T##i
#define MEM_FN_TEMPLATE_INVOCATION(r, data, i, elem) BOOST_PP_COMMA_IF(i) T##i

#define DEFINE_FN2_TMPL( func, tmpl ) \
    template< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_DECLARATION, _, tmpl ) > \
    DEFINE_FN2( func< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_INVOCATION, _, tmpl ) >, fn_ ## func )

// See comments above for DEFINE_MEM_FN
#define DEFINE_MEM_FN_TMPL( func, tmpl ) \
	template< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_DECLARATION, _, tmpl ) > \
	struct [[nodiscard]] dot_mem_fn_ ## func { \
		DEFINE_MEM_FN_BODY_( .template func< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_INVOCATION, _, tmpl ) > ) \
	}; \
	template< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_DECLARATION, _, tmpl ) > \
	struct [[nodiscard]] mem_fn_ ## func { \
		DEFINE_MEM_FN_AUTO_BODY_( template func< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_INVOCATION, _, tmpl ) > ) \
	};

#define DEFINE_FN_TMPL( func, tmpl ) \
	template< BOOST_PP_SEQ_FOR_EACH_I( MEM_FN_TEMPLATE_DECLARATION, _, tmpl ) > \
	static void func(tc::define_fn_dummy_t) noexcept = delete; \
	DEFINE_FN2_TMPL( func, tmpl ) \
	DEFINE_MEM_FN_TMPL( func, tmpl )

DEFINE_FN2( operator delete, fn_operator_delete )

// Cannot use DEFINE_FN2_TMPL here, since casts are build in and the compiler
//(both clang and MSVC) does not accept passing a parameter pack to *_cast
#pragma push_macro("DEFINE_CAST_")
#define DEFINE_CAST_(name, constexpr_) \
	namespace no_adl { \
		template <typename To> \
		struct [[nodiscard]] fn_ ## name { \
			template<typename From> constexpr_ auto operator()(From&& from) const \
				return_decltype_xvalue_by_ref_MAYTHROW( name <To>(tc_move_if_owned(from))) \
		}; \
	} \
	using no_adl::fn_ ## name;

namespace tc {
	DEFINE_CAST_(static_cast, constexpr)
	DEFINE_CAST_(reinterpret_cast, /*constexpr*/)
	DEFINE_CAST_(const_cast, constexpr)
}

#pragma pop_macro("DEFINE_CAST_")

namespace tc::mem_fn_adl {
	template<typename Lambda>
	struct TC_EMPTY_BASES wrap : Lambda {
		static_assert( tc::empty_type<Lambda> );
		constexpr wrap() noexcept = default;
		constexpr explicit wrap(Lambda lambda) noexcept {}
		// decltype(auto) forces the compiler to look into the function to determine the result type. This ensures the static_assert triggers. MSVC just crashed when returning void.
		friend decltype(auto) returns_reference_to_argument(wrap) noexcept {
			static_assert(tc::dependent_false<Lambda>::value, "Choose between tc_mem_fn_xvalue_by_ref and tc_mem_fn_xvalue_by_val");
		}
	};

	template<typename Lambda>
	struct TC_EMPTY_BASES wrap_xvalue_by_ref : wrap<Lambda> {
		using wrap<Lambda>::wrap;
		friend tc::constant<true> returns_reference_to_argument(wrap_xvalue_by_ref); // mark as returning reference to argument
	};
	template<typename Lambda> wrap_xvalue_by_ref(Lambda) -> wrap_xvalue_by_ref<Lambda>;
}

// We can use tc_mem_fn instead of std::mem_fn when the member function has multiple overloads, for example accessors.
// Note: if you want to capture something in the parameter list, you should not use this macro but write your own lambda and make sure it's safe.

#define TC_MEM_FN_IMPL(return_decltype_xxx, ...) \
	[](auto&& _, auto&&... args) return_decltype_xxx( \
		tc_move_if_owned(_)__VA_ARGS__(tc_move_if_owned(args)...) \
	)

#define tc_mem_fn(...) tc::mem_fn_adl::wrap{TC_MEM_FN_IMPL(return_decltype_MAYTHROW, __VA_ARGS__)}
#define tc_mem_fn_xvalue_by_ref(...) tc::mem_fn_adl::wrap_xvalue_by_ref{TC_MEM_FN_IMPL(return_decltype_xvalue_by_ref_MAYTHROW, __VA_ARGS__)}
#define tc_mem_fn_xvalue_by_val(...) tc::mem_fn_adl::wrap{TC_MEM_FN_IMPL(return_decltype_xvalue_by_val_MAYTHROW, __VA_ARGS__)}
#define tc_fn(...) tc::mem_fn_adl::wrap_xvalue_by_ref{[](auto&&... args) return_decltype_xvalue_by_ref_MAYTHROW(__VA_ARGS__(tc_move_if_owned(args)...))}

#define TC_MEMBER_IMPL(return_decltype_xxx, ...) \
	[](auto&& _) return_decltype_xxx(tc_move_if_owned(_)__VA_ARGS__)

#define tc_member(...) tc::mem_fn_adl::wrap{TC_MEMBER_IMPL(return_decltype_noexcept, __VA_ARGS__)}
#define tc_member_xvalue_by_ref(...) tc::mem_fn_adl::wrap_xvalue_by_ref{TC_MEMBER_IMPL(return_decltype_xvalue_by_ref_noexcept, __VA_ARGS__)}
#define tc_member_xvalue_by_val(...) tc::mem_fn_adl::wrap{TC_MEMBER_IMPL(return_decltype_xvalue_by_val_noexcept, __VA_ARGS__)}
#define tc_member_noexcept(...) tc::mem_fn_adl::wrap{TC_MEMBER_IMPL(return_decltype_NOEXCEPT, __VA_ARGS__)}

namespace tc {
	namespace no_adl {
		struct [[nodiscard]] fn_subscript final {
			template<typename Lhs, typename Rhs>
			auto operator()( Lhs&& lhs, Rhs&& rhs ) const&
				return_decltype_xvalue_by_ref_MAYTHROW( tc_move_if_owned(lhs)[tc_move_if_owned(rhs)] )
		};
	}
	using no_adl::fn_subscript;

#pragma push_macro("PREFIX_FN_")
#define PREFIX_FN_( name, op ) \
	namespace fn_ ## name ## _adl { \
		struct [[nodiscard]] fn_ ## name { \
			template<typename T> \
			constexpr auto operator()( T&& t ) const \
				return_decltype_xvalue_by_ref_MAYTHROW(op tc_move_if_owned(t)) \
		}; \
	} \
	using fn_ ## name ## _adl::fn_ ## name;

	PREFIX_FN_( logical_not, ! )
	PREFIX_FN_( indirection, * )
	PREFIX_FN_( increment, ++ )
	PREFIX_FN_( decrement, -- )

	namespace fn_indirection_adl {
		tc::constant<true> returns_reference_to_argument(fn_indirection) noexcept; // mark as returning reference to argument
	}
	namespace fn_increment_adl {
		tc::constant<true> returns_reference_to_argument(fn_increment) noexcept; // mark as returning reference to argument
	}
	namespace fn_decrement_adl {
		tc::constant<true> returns_reference_to_argument(fn_decrement) noexcept; // mark as returning reference to argument
	}

#pragma pop_macro("PREFIX_FN_")

	namespace no_adl {
		template<typename... F>
		struct [[nodiscard]] TC_EMPTY_BASES overload : tc::decay_t<F>... {
			using tc::decay_t<F>::operator()...;
			constexpr overload(F&&... f) noexcept : tc::decay_t<F>(tc_move_if_owned(f))... {}
		};
	}
	using no_adl::overload;

	template <typename... F>
	[[nodiscard]] constexpr auto make_overload(F&&... f) noexcept {
		return overload<F...>(tc_move_if_owned(f)...);
	}

#pragma push_macro("INFIX_FN_")
#define INFIX_FN_( name, op ) \
	namespace no_adl { \
		struct [[nodiscard]] fn_ ## name { \
			template<typename Lhs, typename Rhs> \
			constexpr auto operator()( Lhs&& lhs, Rhs&& rhs ) const \
				return_decltype_xvalue_by_ref_MAYTHROW(tc_move_if_owned(lhs) op tc_move_if_owned(rhs)) \
		}; \
	} \
	using no_adl::fn_ ## name;

	INFIX_FN_( bit_or, | )
	INFIX_FN_( bit_and, & )
	INFIX_FN_( assign_bit_or, |= )
	INFIX_FN_( assign_bit_and, &= )
	INFIX_FN_( plus, + )
	INFIX_FN_( minus, - )
	INFIX_FN_( mul, * )
	INFIX_FN_( logical_or, || )
	// tc::fn_div defined in round.h
	INFIX_FN_( assign_plus, += )
	INFIX_FN_( assign_minus, -= )
	INFIX_FN_( assign_mul, *= )
	// tc::fn_assign_div defined in round.h
	INFIX_FN_( assign, = )

#pragma pop_macro("INFIX_FN_")

#pragma push_macro("FN_LOGICAL_")
#define FN_LOGICAL_(name, op) \
	namespace no_adl { \
		struct [[nodiscard]] fn_assign_logical_ ## name { \
			template<typename Lhs, typename Rhs> \
			auto operator()( Lhs&& lhs, Rhs&& rhs ) const noexcept(noexcept(lhs = lhs op tc_move_if_owned(rhs))) code_return_decltype_xvalue_by_ref( \
				lhs = lhs op tc_move_if_owned(rhs);, tc_move_if_owned(lhs) \
			)\
		}; \
	} \
	using no_adl::fn_assign_logical_ ## name;

	FN_LOGICAL_(or, ||)
	FN_LOGICAL_(and, &&)
}

#pragma pop_macro("FN_LOGICAL_")
