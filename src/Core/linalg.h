// linalg.h - v2.0 - Single-header public domain linear algebra library
//
// The intent of this library is to provide the bulk of the functionality
// you need to write programs that frequently use small, fixed-size vectors
// and matrices, in domains such as computational geometry or computer
// graphics. It strives for terse, readable source code.
//
// The original author of this software is Sterling Orsten, and its permanent
// home is <http://github.com/sgorsten/linalg/>. If you find this software
// useful, an acknowledgement in your source text and/or product documentation
// is appreciated, but not required.
//
// The author acknowledges significant insights and contributions by:
//     Stan Melax <http://github.com/melax/>
//     Dimitri Diakopoulos <http://github.com/ddiakopoulos/>



// This is free and unencumbered software released into the public domain.
// 
// Anyone is free to copy, modify, publish, use, compile, sell, or
// distribute this software, either in source code form or as a compiled
// binary, for any purpose, commercial or non-commercial, and by any
// means.
// 
// In jurisdictions that recognize copyright laws, the author or authors
// of this software dedicate any and all copyright interest in the
// software to the public domain. We make this dedication for the benefit
// of the public at large and to the detriment of our heirs and
// successors. We intend this dedication to be an overt act of
// relinquishment in perpetuity of all present and future rights to this
// software under copyright law.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
// OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
// 
// For more information, please refer to <http://unlicense.org/>

#pragma once

#ifndef LINALG_H
#define LINALG_H

#include <cmath>        // For various unary math functions, such as std::sqrt
#include <cstdlib>      // To resolve std::abs ambiguity on clang
#include <cstdint>      // For implementing namespace linalg::aliases
#include <array>        // For std::array
#include <iosfwd>       // For forward definitions of std::ostream
#include <type_traits>  // For std::enable_if, std::is_same, std::declval

// In Visual Studio 2015, `constexpr` applied to a member function implies `const`, which causes ambiguous overload resolution
#if _MSC_VER <= 1900
#define LINALG_CONSTEXPR14
#else
#define LINALG_CONSTEXPR14 constexpr
#endif

namespace linalg
{
    // Small, fixed-length vector type, consisting of exactly M elements of type T, and presumed to be a column-vector unless otherwise noted.
    template<class T, int M> struct vec;

    // Small, fixed-size matrix type, consisting of exactly M rows and N columns of type T, stored in column-major order.
    template<class T, int M, int N> struct mat;

    // Specialize converter<T,U> with a function application operator that converts type U to type T to enable implicit conversions
    template<class T, class U> struct converter {};
    namespace detail
    {
        template<class T, class U> using conv_t = typename std::enable_if < !std::is_same<T, U>::value, decltype(converter<T, U>{}(std::declval<U>())) > ::type;

        // Trait for retrieving scalar type of any linear algebra object
        template<class A> struct scalar_type {};
        template<class T, int M       > struct scalar_type<vec<T, M  >> { using type = T; };
        template<class T, int M, int N> struct scalar_type<mat<T, M, N>> { using type = T; };

        // Type returned by the compare(...) function which supports all six comparison operators against 0
        template<class T> struct ord { T a, b; };
        template<class T> constexpr bool operator == (const ord<T>& _o, std::nullptr_t) { return _o.a == _o.b; }
        template<class T> constexpr bool operator != (const ord<T>& _o, std::nullptr_t) { return !(_o.a == _o.b); }
        template<class T> constexpr bool operator < (const ord<T>& _o, std::nullptr_t) { return _o.a < _o.b; }
        template<class T> constexpr bool operator > (const ord<T>& _o, std::nullptr_t) { return _o.b < _o.a; }
        template<class T> constexpr bool operator <= (const ord<T>& _o, std::nullptr_t) { return !(_o.b < _o.a); }
        template<class T> constexpr bool operator >= (const ord<T>& _o, std::nullptr_t) { return !(_o.a < _o.b); }

        // Patterns which can be used with the compare(...) function
        template<class A, class B> struct any_compare {};
        template<class T> struct any_compare<vec<T, 1>, vec<T, 1>> { using type = ord<T>; constexpr ord<T> operator() (const vec<T, 1>& _a, const vec<T, 1>& _b) const { return ord<T>{_a.x, _b.x}; } };
        template<class T> struct any_compare<vec<T, 2>, vec<T, 2>> { using type = ord<T>; constexpr ord<T> operator() (const vec<T, 2>& _a, const vec<T, 2>& _b) const { return !(_a.x == _b.x) ? ord<T>{_a.x, _b.x} : ord<T>{ _a.y,_b.y }; } };
        template<class T> struct any_compare<vec<T, 3>, vec<T, 3>> { using type = ord<T>; constexpr ord<T> operator() (const vec<T, 3>& _a, const vec<T, 3>& _b) const { return !(_a.x == _b.x) ? ord<T>{_a.x, _b.x} : !(_a.y == _b.y) ? ord<T>{_a.y, _b.y} : ord<T>{ _a.z,_b.z }; } };
        template<class T> struct any_compare<vec<T, 4>, vec<T, 4>> { using type = ord<T>; constexpr ord<T> operator() (const vec<T, 4>& _a, const vec<T, 4>& _b) const { return !(_a.x == _b.x) ? ord<T>{_a.x, _b.x} : !(_a.y == _b.y) ? ord<T>{_a.y, _b.y} : !(_a.z == _b.z) ? ord<T>{_a.z, _b.z} : ord<T>{ _a.w,_b.w }; } };
        template<class T, int M> struct any_compare<mat<T, M, 1>, mat<T, M, 1>> { using type = ord<T>; constexpr ord<T> operator() (const mat<T, M, 1>& _a, const mat<T, M, 1>& _b) const { return compare(_a.x, _b.x); } };
        template<class T, int M> struct any_compare<mat<T, M, 2>, mat<T, M, 2>> { using type = ord<T>; constexpr ord<T> operator() (const mat<T, M, 2>& _a, const mat<T, M, 2>& _b) const { return _a.x != _b.x ? compare(_a.x, _b.x) : compare(_a.y, _b.y); } };
        template<class T, int M> struct any_compare<mat<T, M, 3>, mat<T, M, 3>> { using type = ord<T>; constexpr ord<T> operator() (const mat<T, M, 3>& _a, const mat<T, M, 3>& _b) const { return _a.x != _b.x ? compare(_a.x, _b.x) : _a.y != _b.y ? compare(_a.y, _b.y) : compare(_a.z, _b.z); } };
        template<class T, int M> struct any_compare<mat<T, M, 4>, mat<T, M, 4>> { using type = ord<T>; constexpr ord<T> operator() (const mat<T, M, 4>& _a, const mat<T, M, 4>& _b) const { return _a.x != _b.x ? compare(_a.x, _b.x) : _a.y != _b.y ? compare(_a.y, _b.y) : _a.z != _b.z ? compare(_a.z, _b.z) : compare(_a.w, _b.w); } };

        // Helper for compile-time index-based access to members of vector and matrix types
        template<int I> struct getter;
        template<> struct getter<0> { template<class A> constexpr auto operator() (A& _a) const -> decltype(_a.x) { return _a.x; } };
        template<> struct getter<1> { template<class A> constexpr auto operator() (A& _a) const -> decltype(_a.y) { return _a.y; } };
        template<> struct getter<2> { template<class A> constexpr auto operator() (A& _a) const -> decltype(_a.z) { return _a.z; } };
        template<> struct getter<3> { template<class A> constexpr auto operator() (A& _a) const -> decltype(_a.w) { return _a.w; } };

        // Stand-in for std::integer_sequence/std::make_integer_sequence
        template<int... I> struct seq {};
        template<int A, int N> struct make_seq_impl;
        template<int A> struct make_seq_impl<A, 0> { using type = seq<>; };
        template<int A> struct make_seq_impl<A, 1> { using type = seq<A + 0>; };
        template<int A> struct make_seq_impl<A, 2> { using type = seq<A + 0, A + 1>; };
        template<int A> struct make_seq_impl<A, 3> { using type = seq<A + 0, A + 1, A + 2>; };
        template<int A> struct make_seq_impl<A, 4> { using type = seq<A + 0, A + 1, A + 2, A + 3>; };
        template<int A, int B> using make_seq = typename make_seq_impl<A, B - A>::type;
        template<class T, int M, int... I> vec<T, sizeof...(I)> constexpr swizzle(const vec<T, M>& _v, seq<I...>) { return { getter<I>{}(_v)... }; }
        template<class T, int M, int N, int... I, int... J> mat<T, sizeof...(I), sizeof...(J)> constexpr swizzle(const mat<T, M, N>& _m, seq<I...> _i, seq<J...> _j) { return { swizzle(getter<J>{}(_m),_i)... }; }

        // SFINAE helpers to determine result of function application
        template<class F, class... T> using ret_t = decltype(std::declval<F>()(std::declval<T>()...));

        // SFINAE helper which is defined if all provided types are scalars
        struct empty {};
        template<class... T> struct scalars;
        template<> struct scalars<> { using type = void; };
        template<class T, class... U> struct scalars<T, U...> : std::conditional<std::is_arithmetic<T>::value, scalars<U...>, empty>::type {};
        template<class... T> using scalars_t = typename scalars<T...>::type;

        // Helpers which indicate how apply(F, ...) should be called for various arguments
        template<class F, class Void, class... T> struct apply {}; // Patterns which contain only vectors or scalars
        template<class F, int M, class A                  > struct apply<F, scalars_t<   >, vec<A, M>                    > { using type = vec<ret_t<F, A    >, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a) { return { _f(getter<I>{}(_a))... }; } };
        template<class F, int M, class A, class B         > struct apply<F, scalars_t<   >, vec<A, M>, vec<B, M>          > { using type = vec<ret_t<F, A, B  >, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, const vec<B, M>& _b) { return { _f(getter<I>{}(_a), getter<I>{}(_b))... }; } };
        template<class F, int M, class A, class B         > struct apply<F, scalars_t<B  >, vec<A, M>, B                 > { using type = vec<ret_t<F, A, B  >, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, B                _b) { return { _f(getter<I>{}(_a), _b)... }; } };
        template<class F, int M, class A, class B         > struct apply<F, scalars_t<A  >, A, vec<B, M>          > { using type = vec<ret_t<F, A, B  >, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, A                a, const vec<B, M>& _b) { return { _f(_a,              getter<I>{}(_b))... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<   >, vec<A, M>, vec<B, M>, vec<C, M>> { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, const vec<B, M>& _b, const vec<C, M>& _c) { return { _f(getter<I>{}(_a), getter<I>{}(_b), getter<I>{}(_c))... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<C  >, vec<A, M>, vec<B, M>, C       > { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, const vec<B, M>& _b, C                _c) { return { _f(getter<I>{}(_a), getter<I>{}(_b), _c)... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<B  >, vec<A, M>, B, vec<C, M>> { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, B                _b, const vec<C, M>& _c) { return { _f(getter<I>{}(_a), _b,              getter<I>{}(_c))... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<B, C>, vec<A, M>, B, C       > { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, const vec<A, M>& _a, B                _b, C                _c) { return { _f(getter<I>{}(_a), _b,              _c)... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<A  >, A, vec<B, M>, vec<C, M>> { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, A                _a, const vec<B, M>& _b, const vec<C, M>& _c) { return { _f(_a,              getter<I>{}(_b), getter<I>{}(_c))... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<A, C>, A, vec<B, M>, C       > { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, A                _a, const vec<B, M>& _b, C                _c) { return { _f(_a,              getter<I>{}(_b), _c)... }; } };
        template<class F, int M, class A, class B, class C> struct apply<F, scalars_t<A, B>, A, B, vec<C, M>> { using type = vec<ret_t<F, A, B, C>, M>; enum { size = M }; template<int... I> static constexpr type impl(seq<I...>, F _f, A                _a, B                _b, const vec<C, M>& _c) { return { _f(_a,              _b,              getter<I>{}(_c))... }; } };
        template<class F, int M, int N, class A         > struct apply<F, scalars_t< >, mat<A, M, N>            > { using type = mat<ret_t<F, A  >, M, N>; enum { size = N }; template<int... J> static constexpr type impl(seq<J...>, F _f, const mat<A, M, N>& _a) { return { apply<F, void, vec<A,M>          >::impl(make_seq<0,M>{}, _f, getter<J>{}(_a))... }; } };
        template<class F, int M, int N, class A, class B> struct apply<F, scalars_t< >, mat<A, M, N>, mat<B, M, N>> { using type = mat<ret_t<F, A, B>, M, N>; enum { size = N }; template<int... J> static constexpr type impl(seq<J...>, F _f, const mat<A, M, N>& _a, const mat<B, M, N>& _b) { return { apply<F, void, vec<A,M>, vec<B,M>>::impl(make_seq<0,M>{}, _f, getter<J>{}(_a), getter<J>{}(_b))... }; } };
        template<class F, int M, int N, class A, class B> struct apply<F, scalars_t<B>, mat<A, M, N>, B         > { using type = mat<ret_t<F, A, B>, M, N>; enum { size = N }; template<int... J> static constexpr type impl(seq<J...>, F _f, const mat<A, M, N>& _a, B                  _b) { return { apply<F, void, vec<A,M>, B       >::impl(make_seq<0,M>{}, _f, getter<J>{}(_a), _b)... }; } };
        template<class F, int M, int N, class A, class B> struct apply<F, scalars_t<A>, A, mat<B, M, N>> { using type = mat<ret_t<F, A, B>, M, N>; enum { size = N }; template<int... J> static constexpr type impl(seq<J...>, F _f, A                  _a, const mat<B, M, N>& _b) { return { apply<F, void, A,        vec<B,M>>::impl(make_seq<0,M>{}, _f, _a,              getter<J>{}(_b))... }; } };
        template<class F, class... A> struct apply<F, scalars_t<A...>, A...> { using type = ret_t<F, A...>; enum { size = 0 }; static constexpr type impl(seq<>, F _f, A... _a) { return _f(_a...); } };

        // Function objects for selecting between alternatives
        struct min { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> typename std::remove_reference<decltype(_a < _b ? _a : _b)>::type{ return _a < _b ? _a : _b; } };
        struct max { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> typename std::remove_reference<decltype(_a < _b ? _b : _a)>::type{ return _a < _b ? _b : _a; } };
        struct clamp { template<class A, class B, class C> constexpr auto operator() (A _a, B _b, C _c) const -> typename std::remove_reference<decltype(_a < _b ? _b : _a < _c ? _a : _c)>::type{ return _a < _b ? _b : _a < _c ? _a : _c; } };
        struct select { template<class A, class B, class C> constexpr auto operator() (A _a, B _b, C _c) const -> typename std::remove_reference<decltype(_a ? _b : _c)>::type { return _a ? _b : _c; } };
        struct lerp { template<class A, class B, class C> constexpr auto operator() (A _a, B _b, C _c) const -> decltype(_a* (1 - _c) + _b * _c) { return _a * (1 - _c) + _b * _c; } };

        // Function objects for applying operators
        struct op_pos { template<class A> constexpr auto operator() (A _a) const -> decltype(+_a) { return +_a; } };
        struct op_neg { template<class A> constexpr auto operator() (A _a) const -> decltype(-_a) { return -_a; } };
        struct op_not { template<class A> constexpr auto operator() (A _a) const -> decltype(!_a) { return !_a; } };
        struct op_cmp { template<class A> constexpr auto operator() (A _a) const -> decltype(~(_a)) { return ~_a; } };
        struct op_mul { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a* _b) { return _a * _b; } };
        struct op_div { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a / _b) { return _a / _b; } };
        struct op_mod { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a% _b) { return _a % _b; } };
        struct op_add { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a + _b) { return _a + _b; } };
        struct op_sub { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a - _b) { return _a - _b; } };
        struct op_lsh { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a << _b) { return _a << _b; } };
        struct op_rsh { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a >> _b) { return _a >> _b; } };
        struct op_lt { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a < _b) { return _a < _b; } };
        struct op_gt { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a > _b) { return _a > _b; } };
        struct op_le { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a <= _b) { return _a <= _b; } };
        struct op_ge { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a >= _b) { return _a >= _b; } };
        struct op_eq { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a == _b) { return _a == _b; } };
        struct op_ne { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a != _b) { return _a != _b; } };
        struct op_int { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a & _b) { return _a & _b; } };
        struct op_xor { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a ^ _b) { return _a ^ _b; } };
        struct op_un { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a | _b) { return _a | _b; } };
        struct op_and { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a && _b) { return _a && _b; } };
        struct op_or { template<class A, class B> constexpr auto operator() (A _a, B _b) const -> decltype(_a || _b) { return _a || _b; } };

        // Function objects for applying standard library math functions
        struct std_abs { template<class A> auto operator() (A _a) const -> decltype(std::abs(_a)) { return std::abs(_a); } };
        struct std_floor { template<class A> auto operator() (A _a) const -> decltype(std::floor(_a)) { return std::floor(_a); } };
        struct std_ceil { template<class A> auto operator() (A _a) const -> decltype(std::ceil(_a)) { return std::ceil(_a); } };
        struct std_exp { template<class A> auto operator() (A _a) const -> decltype(std::exp(_a)) { return std::exp(_a); } };
        struct std_log { template<class A> auto operator() (A _a) const -> decltype(std::log(_a)) { return std::log(_a); } };
        struct std_log10 { template<class A> auto operator() (A _a) const -> decltype(std::log10(_a)) { return std::log10(_a); } };
        struct std_sqrt { template<class A> auto operator() (A _a) const -> decltype(std::sqrt(_a)) { return std::sqrt(_a); } };
        struct std_sin { template<class A> auto operator() (A _a) const -> decltype(std::sin(_a)) { return std::sin(_a); } };
        struct std_cos { template<class A> auto operator() (A _a) const -> decltype(std::cos(_a)) { return std::cos(_a); } };
        struct std_tan { template<class A> auto operator() (A _a) const -> decltype(std::tan(_a)) { return std::tan(_a); } };
        struct std_asin { template<class A> auto operator() (A _a) const -> decltype(std::asin(_a)) { return std::asin(_a); } };
        struct std_acos { template<class A> auto operator() (A _a) const -> decltype(std::acos(_a)) { return std::acos(_a); } };
        struct std_atan { template<class A> auto operator() (A _a) const -> decltype(std::atan(_a)) { return std::atan(_a); } };
        struct std_sinh { template<class A> auto operator() (A _a) const -> decltype(std::sinh(_a)) { return std::sinh(_a); } };
        struct std_cosh { template<class A> auto operator() (A _a) const -> decltype(std::cosh(_a)) { return std::cosh(_a); } };
        struct std_tanh { template<class A> auto operator() (A _a) const -> decltype(std::tanh(_a)) { return std::tanh(_a); } };
        struct std_round { template<class A> auto operator() (A _a) const -> decltype(std::round(_a)) { return std::round(_a); } };
        struct std_fmod { template<class A, class B> auto operator() (A _a, B _b) const -> decltype(std::fmod(_a, _b)) { return std::fmod(_a, _b); } };
        struct std_pow { template<class A, class B> auto operator() (A _a, B _b) const -> decltype(std::pow(_a, _b)) { return std::pow(_a, _b); } };
        struct std_atan2 { template<class A, class B> auto operator() (A _a, B _b) const -> decltype(std::atan2(_a, _b)) { return std::atan2(_a, _b); } };
        struct std_copysign { template<class A, class B> auto operator() (A _a, B _b) const -> decltype(std::copysign(_a, _b)) { return std::copysign(_a, _b); } };
    }

    // Small, fixed-length vector type, consisting of exactly M elements of type T, and presumed to be a column-vector unless otherwise noted
    template<class T> struct vec<T, 1>
    {
        T                           x;
        constexpr                   vec() : x() {}
        constexpr                   vec(const T& _x) : x(_x) {}
        // NOTE: vec<T,1> does NOT have a constructor from pointer, this can conflict with initializing its single element from zero
        template<class U>
        constexpr explicit          vec(const vec<U, 1>& _v) : vec(static_cast<T>(_v.x)) {}
        constexpr const T& operator[] (int) const { return x; }
        LINALG_CONSTEXPR14 T& operator[] (int) { return x; }

        template<class U, class = detail::conv_t<vec, U>> constexpr vec(const U& _u) : vec(converter<vec, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, vec>> constexpr operator U () const { return converter<U, vec>{}(*this); }
    };
    template<class T> struct vec<T, 2>
    {
        T                           x, y;
        constexpr                   vec() : x(), y() {}
        constexpr                   vec(const T& _x, const T& _y) : x(_x), y(_y) {}
        constexpr explicit          vec(const T& _s) : vec(_s, _s) {}
        constexpr explicit          vec(const T* _p) : vec(_p[0], _p[1]) {}
        template<class U>
        constexpr explicit          vec(const vec<U, 2>& _v) : vec(static_cast<T>(_v.x), static_cast<T>(_v.y)) {}
        constexpr const T& operator[] (int _i) const { return _i == 0 ? x : y; }
        LINALG_CONSTEXPR14 T& operator[] (int _i) { return _i == 0 ? x : y; }

        template<class U, class = detail::conv_t<vec, U>> constexpr vec(const U& _u) : vec(converter<vec, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, vec>> constexpr operator U () const { return converter<U, vec>{}(*this); }
    };
    template<class T> struct vec<T, 3>
    {
        T                           x, y, z;
        constexpr                   vec() : x(), y(), z() {}
        constexpr                   vec(const T& _x, const T& _y, const T& _z) : x(_x), y(_y), z(_z) {}
        constexpr                   vec(const vec<T, 2>& _xy, const T& _z) : vec(_xy.x, _xy.y, _z) {}
        constexpr explicit          vec(const T& _s) : vec(_s, _s, _s) {}
        constexpr explicit          vec(const T* _p) : vec(_p[0], _p[1], _p[2]) {}
        template<class U>
        constexpr explicit          vec(const vec<U, 3>& _v) : vec(static_cast<T>(_v.x), static_cast<T>(_v.y), static_cast<T>(_v.z)) {}
        constexpr const T& operator[] (int _i) const { return _i == 0 ? x : _i == 1 ? y : z; }
        LINALG_CONSTEXPR14 T& operator[] (int _i) { return _i == 0 ? x : _i == 1 ? y : z; }
        constexpr const vec<T, 2>& xy() const { return *reinterpret_cast<const vec<T, 2>*>(this); }
        vec<T, 2>& xy() { return *reinterpret_cast<vec<T, 2>*>(this); }
        constexpr void assign(float(&_array)[3]) const { _array[0] = x; _array[1] = y; _array[2] = z; }

        template<class U, class = detail::conv_t<vec, U>> constexpr vec(const U& _u) : vec(converter<vec, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, vec>> constexpr operator U () const { return converter<U, vec>{}(*this); }
    };
    template<class T> struct vec<T, 4>
    {
        T                           x, y, z, w;
        constexpr                   vec() : x(), y(), z(), w() {}
        constexpr                   vec(const T& _x, const T& _y, const T& _z, const T& _w) : x(_x), y(_y), z(_z), w(_w) {}
        constexpr                   vec(const vec<T, 2>& _xy, const T& _z, const T& _w) : vec(_xy.x, _xy.y, _z, _w) {}
        constexpr                   vec(const vec<T, 3>& _xyz, const T& _w) : vec(_xyz.x, _xyz.y, _xyz.z, _w) {}
        constexpr explicit          vec(const T& _s) : vec(_s, _s, _s, _s) {}
        constexpr explicit          vec(const T* _p) : vec(_p[0], _p[1], _p[2], _p[3]) {}
        template<class U>
        constexpr explicit          vec(const vec<U, 4>& _v) : vec(static_cast<T>(_v.x), static_cast<T>(_v.y), static_cast<T>(_v.z), static_cast<T>(_v.w)) {}
        constexpr const T& operator[] (int _i) const { return _i == 0 ? x : _i == 1 ? y : _i == 2 ? z : w; }
        LINALG_CONSTEXPR14 T& operator[] (int _i) { return _i == 0 ? x : _i == 1 ? y : _i == 2 ? z : w; }
        constexpr const vec<T, 2>& xy() const { return *reinterpret_cast<const vec<T, 2>*>(this); }
        constexpr const vec<T, 3>& xyz() const { return *reinterpret_cast<const vec<T, 3>*>(this); }
        vec<T, 2>& xy() { return *reinterpret_cast<vec<T, 2>*>(this); }
        vec<T, 3>& xyz() { return *reinterpret_cast<vec<T, 3>*>(this); }

        template<class U, class = detail::conv_t<vec, U>> constexpr vec(const U& _u) : vec(converter<vec, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, vec>> constexpr operator U () const { return converter<U, vec>{}(*this); }
    };

    // Small, fixed-size matrix type, consisting of exactly M rows and N columns of type T, stored in column-major order.
    template<class T, int M> struct mat<T, M, 1>
    {
        typedef vec<T, M>            V;
        V                           x;
        constexpr                   mat() : x() {}
        constexpr                   mat(const V& _x) : x(_x_) {}
        constexpr explicit          mat(const T& _s) : x(_s) {}
        constexpr explicit          mat(const T* _p) : x(_p + M * 0) {}
        template<class U>
        constexpr explicit          mat(const mat<U, M, 1>& _m) : mat(V(_m.x)) {}
        constexpr vec<T, 1>          row(int _i) const { return { x[_i] }; }
        constexpr const V& operator[] (int _j) const { return x; }
        LINALG_CONSTEXPR14 V& operator[] (int _j) { return x; }

        template<class U, class = detail::conv_t<mat, U>> constexpr mat(const U& _u) : mat(converter<mat, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, mat>> constexpr operator U () const { return converter<U, mat>{}(*this); }
    };
    template<class T, int M> struct mat<T, M, 2>
    {
        typedef vec<T, M>            V;
        V                           x, y;
        constexpr                   mat() : x(), y() {}
        constexpr                   mat(const V& _x, const V& _y) : x(_x), y(_y) {}
        constexpr explicit          mat(const T& _s) : x(_s), y(_s) {}
        constexpr explicit          mat(const T* _p) : x(_p + M * 0), y(_p + M * 1) {}
        template<class U>
        constexpr explicit          mat(const mat<U, M, 2>& _m) : mat(V(_m.x), V(_m.y)) {}
        constexpr vec<T, 2>         row(int _i) const { return { x[_i], y[_i] }; }
        constexpr const V& operator[] (int _j) const { return _j == 0 ? x : y; }
        LINALG_CONSTEXPR14 V& operator[] (int _j) { return _j == 0 ? x : y; }

        template<class U, class = detail::conv_t<mat, U>> constexpr mat(const U& _u) : mat(converter<mat, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, mat>> constexpr operator U () const { return converter<U, mat>{}(*this); }
    };
    template<class T, int M> struct mat<T, M, 3>
    {
        typedef vec<T, M>            V;
        V                           x, y, z;
        constexpr                   mat() : x(), y(), z() {}
        constexpr                   mat(const V& _x, const V& _y, const V& _z) : x(_x), y(_y), z(_z) {}
        constexpr explicit          mat(const T& _s) : x(_s), y(_s), z(_s) {}
        constexpr explicit          mat(const T* _p) : x(_p + M * 0), y(_p + M * 1), z(_p + M * 2) {}
        template<class U>
        constexpr explicit          mat(const mat<U, M, 3>& _m) : mat(V(_m.x), V(_m.y), V(_m.z)) {}
        constexpr vec<T, 3>         row(int _i) const { return { x[_i], y[_i], z[_i] }; }
        constexpr const V& operator[] (int _j) const { return _j == 0 ? x : _j == 1 ? y : z; }
        LINALG_CONSTEXPR14 V& operator[] (int _j) { return _j == 0 ? x : _j == 1 ? y : z; }

        template<class U, class = detail::conv_t<mat, U>> constexpr mat(const U& _u) : mat(converter<mat, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, mat>> constexpr operator U () const { return converter<U, mat>{}(*this); }
    };
    template<class T, int M> struct mat<T, M, 4>
    {
        typedef vec<T, M>            V;
        V                           x, y, z, w;
        constexpr                   mat() : x(), y(), z(), w() {}
        constexpr                   mat(const V& _x, const V& _y, const V& _z, const V& _w) : x(_x), y(_y), z(_z), w(_w) {}
        constexpr explicit          mat(const T& _s) : x(_s), y(_s), z(_s), w(_s) {}
        constexpr explicit          mat(const T* _p) : x(_p + M * 0), y(_p + M * 1), z(_p + M * 2), w(_p + M * 3) {}
        template<class U>
        constexpr explicit          mat(const mat<U, M, 4>& _m) : mat(V(_m.x), V(_m.y), V(_m.z), V(_m.w)) {}
        constexpr vec<T, 4>         row(int _i) const { return { x[_i], y[_i], z[_i], w[_i] }; }
        constexpr const V& operator[] (int _j) const { return _j == 0 ? x : _j == 1 ? y : _j == 2 ? z : w; }
        LINALG_CONSTEXPR14 V& operator[] (int _j) { return _j == 0 ? x : _j == 1 ? y : _j == 2 ? z : w; }

        template<class U, class = detail::conv_t<mat, U>> constexpr mat(const U& _u) : mat(converter<mat, U>{}(_u)) {}
        template<class U, class = detail::conv_t<U, mat>> constexpr operator U () const { return converter<U, mat>{}(*this); }
    };

    // Define a type which will convert to the multiplicative identity of any square matrix
    struct identity_t { constexpr explicit identity_t(int) {} };
    template<class T> struct converter<mat<T, 1, 1>, identity_t> { mat<T, 1, 1> operator() (identity_t) const { return { vec<T,1>{1} }; } };
    template<class T> struct converter<mat<T, 2, 2>, identity_t> { mat<T, 2, 2> operator() (identity_t) const { return { {1,0},{0,1} }; } };
    template<class T> struct converter<mat<T, 3, 3>, identity_t> { mat<T, 3, 3> operator() (identity_t) const { return { {1,0,0},{0,1,0},{0,0,1} }; } };
    template<class T> struct converter<mat<T, 4, 4>, identity_t> { mat<T, 4, 4> operator() (identity_t) const { return { {1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1} }; } };
    constexpr identity_t identity{ 1 };

    // Produce a scalar by applying f(A,B) -> A to adjacent pairs of elements from a vec/mat in left-to-right/column-major order (matching the associativity of arithmetic and logical operators)
    template<class F, class A, class B> constexpr A fold(F _f, A _a, const vec<B, 1>& _b) { return _f(_a, _b.x); }
    template<class F, class A, class B> constexpr A fold(F _f, A _a, const vec<B, 2>& _b) { return _f(_f(_a, _b.x), _b.y); }
    template<class F, class A, class B> constexpr A fold(F _f, A _a, const vec<B, 3>& _b) { return _f(_f(_f(_a, _b.x), _b.y), _b.z); }
    template<class F, class A, class B> constexpr A fold(F _f, A _a, const vec<B, 4>& _b) { return _f(_f(_f(_f(_a, _b.x), _b.y), _b.z), _b.w); }
    template<class F, class A, class B, int M> constexpr A fold(F _f, A _a, const mat<B, M, 1>& _b) { return fold(_f, _a, _b.x); }
    template<class F, class A, class B, int M> constexpr A fold(F _f, A _a, const mat<B, M, 2>& _b) { return fold(_f, fold(_f, _a, _b.x), _b.y); }
    template<class F, class A, class B, int M> constexpr A fold(F _f, A _a, const mat<B, M, 3>& _b) { return fold(_f, fold(_f, fold(_f, _a, _b.x), _b.y), _b.z); }
    template<class F, class A, class B, int M> constexpr A fold(F _f, A _a, const mat<B, M, 4>& _b) { return fold(_f, fold(_f, fold(_f, fold(_f, _a, _b.x), _b.y), _b.z), _b.w); }

    // Type aliases for the result of calling apply(...) with various arguments, can be used with return type SFINAE to constrian overload sets
    template<class F, class... A> using apply_t = typename detail::apply<F, void, A...>::type;
    template<class A> using scalar_t = typename detail::scalar_type<A>::type; // Underlying scalar type when performing elementwise operations

    // apply(f,...) applies the provided function in an elementwise fashion to its arguments, producing an object of the same dimensions
    template<class F, class... A> constexpr apply_t<F, A...> apply(F _func, const A& ... _args) { return detail::apply<F, void, A...>::impl(detail::make_seq<0, detail::apply<F, void, A...>::size>{}, _func, _args...); }

    // map(a,f) is equivalent to apply(f,a)
    template<class A, class F> constexpr apply_t<F, A> map(const A& _a, F _func) { return apply(_func, _a); }

    // zip(a,b,f) is equivalent to apply(f,a,b)
    template<class A, class B, class F> constexpr apply_t<F, A, B> zip(const A& _a, const B& _b, F _func) { return apply(_func, _a, _b); }

    // Relational operators are defined to compare the elements of two vectors or matrices lexicographically, in column-major order
    template<class A, class B> constexpr typename detail::any_compare<A, B>::type compare(const A& _a, const B& _b) { return detail::any_compare<A, B>()(_a, _b); }
    template<class A, class B> constexpr auto operator == (const A& _a, const B& _b) -> decltype(compare(_a, _b) == 0) { return compare(_a, _b) == 0; }
    template<class A, class B> constexpr auto operator != (const A& _a, const B& _b) -> decltype(compare(_a, _b) != 0) { return compare(_a, _b) != 0; }
    template<class A, class B> constexpr auto operator <  (const A& _a, const B& _b) -> decltype(compare(_a, _b) < 0) { return compare(_a, _b) < 0; }
    template<class A, class B> constexpr auto operator >  (const A& _a, const B& _b) -> decltype(compare(_a, _b) > 0) { return compare(_a, _b) > 0; }
    template<class A, class B> constexpr auto operator <= (const A& _a, const B& _b) -> decltype(compare(_a, _b) <= 0) { return compare(_a, _b) <= 0; }
    template<class A, class B> constexpr auto operator >= (const A& _a, const B& _b) -> decltype(compare(_a, _b) >= 0) { return compare(_a, _b) >= 0; }

    // Functions for coalescing scalar values
    template<class A> constexpr bool any(const A& _a) { return fold(detail::op_or{}, false, _a); }
    template<class A> constexpr bool all(const A& _a) { return fold(detail::op_and{}, true, _a); }
    template<class A> constexpr scalar_t<A> sum(const A& _a) { return fold(detail::op_add{}, scalar_t<A>(0), _a); }
    template<class A> constexpr scalar_t<A> product(const A& _a) { return fold(detail::op_mul{}, scalar_t<A>(1), _a); }
    template<class A> constexpr scalar_t<A> minelem(const A& _a) { return fold(detail::min{}, _a.x, _a); }
    template<class A> constexpr scalar_t<A> maxelem(const A& _a) { return fold(detail::max{}, _a.x, _a); }
    template<class T, int M> int argmin(const vec<T, M>& _a) { int j = 0; for (int i = 1; i < M; ++i) if (_a[i] < _a[j]) j = i; return j; }
    template<class T, int M> int argmax(const vec<T, M>& _a) { int j = 0; for (int i = 1; i < M; ++i) if (_a[i] > _a[j]) j = i; return j; }

    // Unary operators are defined component-wise for linalg types
    template<class A> constexpr apply_t<detail::op_pos, A> operator + (const A& _a) { return apply(detail::op_pos{}, _a); }
    template<class A> constexpr apply_t<detail::op_neg, A> operator - (const A& _a) { return apply(detail::op_neg{}, _a); }
    template<class A> constexpr apply_t<detail::op_cmp, A> operator ~ (const A& _a) { return apply(detail::op_cmp{}, _a); }
    template<class A> constexpr apply_t<detail::op_not, A> operator ! (const A& _a) { return apply(detail::op_not{}, _a); }

    // Binary operators are defined component-wise for linalg types
    template<class A, class B> constexpr apply_t<detail::op_add, A, B> operator +  (const A& _a, const B& _b) { return apply(detail::op_add{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_sub, A, B> operator -  (const A& _a, const B& _b) { return apply(detail::op_sub{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_mul, A, B> operator *  (const A& _a, const B& _b) { return apply(detail::op_mul{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_div, A, B> operator /  (const A& _a, const B& _b) { return apply(detail::op_div{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_mod, A, B> operator %  (const A& _a, const B& _b) { return apply(detail::op_mod{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_un , A, B> operator |  (const A& _a, const B& _b) { return apply(detail::op_un {}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_xor, A, B> operator ^  (const A& _a, const B& _b) { return apply(detail::op_xor{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_int, A, B> operator &  (const A& _a, const B& _b) { return apply(detail::op_int{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_lsh, A, B> operator << (const A& _a, const B& _b) { return apply(detail::op_lsh{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_rsh, A, B> operator >> (const A& _a, const B& _b) { return apply(detail::op_rsh{}, _a, _b); }

    // Binary assignment operators a $= b is always defined as though it were explicitly written a = a $ b
    template<class A, class B> constexpr auto operator +=  (A& _a, const B& _b) -> decltype(_a = _a + _b) { return _a = _a + _b; }
    template<class A, class B> constexpr auto operator -=  (A& _a, const B& _b) -> decltype(_a = _a - _b) { return _a = _a - _b; }
    template<class A, class B> constexpr auto operator *=  (A& _a, const B& _b) -> decltype(_a = _a * _b) { return _a = _a * _b; }
    template<class A, class B> constexpr auto operator /=  (A& _a, const B& _b) -> decltype(_a = _a / _b) { return _a = _a / _b; }
    template<class A, class B> constexpr auto operator %=  (A& _a, const B& _b) -> decltype(_a = _a % _b) { return _a = _a % _b; }
    template<class A, class B> constexpr auto operator |=  (A& _a, const B& _b) -> decltype(_a = _a | _b) { return _a = _a | _b; }
    template<class A, class B> constexpr auto operator ^=  (A& _a, const B& _b) -> decltype(_a = _a ^ _b) { return _a = _a ^ _b; }
    template<class A, class B> constexpr auto operator &=  (A& _a, const B& _b) -> decltype(_a = _a & _b) { return _a = _a & _b; }
    template<class A, class B> constexpr auto operator <<= (A& _a, const B& _b) -> decltype(_a = _a << _b) { return _a = _a << _b; }
    template<class A, class B> constexpr auto operator >>= (A& _a, const B& _b) -> decltype(_a = _a >> _b) { return _a = _a >> _b; }

    // Swizzles and subobjects
    template<int... I, class T, int M>                              constexpr vec<T, sizeof...(I)>   swizzle(const vec<T, M>& _a) { return { detail::getter<I>(_a)... }; }
    template<int I0, int I1, class T, int M>                        constexpr vec<T, I1 - I0>          subvec(const vec<T, M>& _a) { return detail::swizzle(_a, detail::make_seq<I0, I1>{}); }
    template<int I0, int J0, int I1, int J1, class T, int M, int N> constexpr mat<T, I1 - I0, J1 - J0>    submat(const mat<T, M, N>& _a) { return detail::swizzle(_a, detail::make_seq<I0, I1>{}, detail::make_seq<J0, J1>{}); }

    // Component-wise standard library math functions
    template<class A> apply_t<detail::std_abs, A> abs(const A& _a) { return apply(detail::std_abs{}, _a); }
    template<class A> apply_t<detail::std_floor, A> floor(const A& _a) { return apply(detail::std_floor{}, _a); }
    template<class A> apply_t<detail::std_ceil, A> ceil(const A& _a) { return apply(detail::std_ceil{}, _a); }
    template<class A> apply_t<detail::std_exp, A> exp(const A& _a) { return apply(detail::std_exp{}, _a); }
    template<class A> apply_t<detail::std_log, A> log(const A& _a) { return apply(detail::std_log{}, _a); }
    template<class A> apply_t<detail::std_log10, A> log10(const A& _a) { return apply(detail::std_log10{}, _a); }
    template<class A> apply_t<detail::std_sqrt, A> sqrt(const A& _a) { return apply(detail::std_sqrt{}, _a); }
    template<class A> apply_t<detail::std_sin, A> sin(const A& _a) { return apply(detail::std_sin{}, _a); }
    template<class A> apply_t<detail::std_cos, A> cos(const A& _a) { return apply(detail::std_cos{}, _a); }
    template<class A> apply_t<detail::std_tan, A> tan(const A& _a) { return apply(detail::std_tan{}, _a); }
    template<class A> apply_t<detail::std_asin, A> asin(const A& _a) { return apply(detail::std_asin{}, _a); }
    template<class A> apply_t<detail::std_acos, A> acos(const A& _a) { return apply(detail::std_acos{}, _a); }
    template<class A> apply_t<detail::std_atan, A> atan(const A& _a) { return apply(detail::std_atan{}, _a); }
    template<class A> apply_t<detail::std_sinh, A> sinh(const A& _a) { return apply(detail::std_sinh{}, _a); }
    template<class A> apply_t<detail::std_cosh, A> cosh(const A& _a) { return apply(detail::std_cosh{}, _a); }
    template<class A> apply_t<detail::std_tanh, A> tanh(const A& _a) { return apply(detail::std_tanh{}, _a); }
    template<class A> apply_t<detail::std_round, A> round(const A& _a) { return apply(detail::std_round{}, _a); }

    template<class A, class B> apply_t<detail::std_fmod, A, B> fmod(const A& _a, const B& _b) { return apply(detail::std_fmod{}, _a, _b); }
    template<class A, class B> apply_t<detail::std_pow, A, B> pow(const A& _a, const B& _b) { return apply(detail::std_pow{}, _a, _b); }
    template<class A, class B> apply_t<detail::std_atan2, A, B> atan2(const A& _a, const B& _b) { return apply(detail::std_atan2{}, _a, _b); }
    template<class A, class B> apply_t<detail::std_copysign, A, B> copysign(const A& _a, const B& _b) { return apply(detail::std_copysign{}, _a, _b); }

    // Component-wise relational functions on vectors
    template<class A, class B> constexpr apply_t<detail::op_eq, A, B> equal(const A& _a, const B& _b) { return apply(detail::op_eq{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_ne, A, B> nequal(const A& _a, const B& _b) { return apply(detail::op_ne{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_lt, A, B> less(const A& _a, const B& _b) { return apply(detail::op_lt{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_gt, A, B> greater(const A& _a, const B& _b) { return apply(detail::op_gt{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_le, A, B> lequal(const A& _a, const B& _b) { return apply(detail::op_le{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::op_ge, A, B> gequal(const A& _a, const B& _b) { return apply(detail::op_ge{}, _a, _b); }

    // Component-wise selection functions on vectors
    template<class A, class B> constexpr apply_t<detail::min, A, B> min_(const A& _a, const B& _b) { return apply(detail::min{}, _a, _b); }
    template<class A, class B> constexpr apply_t<detail::max, A, B> max_(const A& _a, const B& _b) { return apply(detail::max{}, _a, _b); }

    template<class X, class L, class H> constexpr apply_t<detail::clamp, X, L, H> clamp(const X& _x, const L& _l, const H& _h) { return apply(detail::clamp{}, _x, _l, _h); }
    template<class P, class A, class B> constexpr apply_t<detail::select, P, A, B> select(const P& _p, const A& _a, const B& _b) { return apply(detail::select{}, _p, _a, _b); }
    template<class A, class B, class T> constexpr apply_t<detail::lerp, A, B, T> lerp(const A& _a, const B& _b, const T& _t) { return apply(detail::lerp{}, _a, _b, _t); }

    // Support for vector algebra
    template<class T> constexpr T        cross(const vec<T, 2>& _a, const vec<T, 2>& _b) { return _a.x * _b.y - _a.y * _b.x; }
    template<class T> constexpr vec<T, 2> cross(T _a, const vec<T, 2>& _b) { return { -_a * _b.y, _a * _b.x }; }
    template<class T> constexpr vec<T, 2> cross(const vec<T, 2>& _a, T _b) { return { _a.y * _b, -_a.x * _b }; }
    template<class T> constexpr vec<T, 3> cross(const vec<T, 3>& _a, const vec<T, 3>& _b) { return { _a.y * _b.z - _a.z * _b.y, _a.z * _b.x - _a.x * _b.z, _a.x * _b.y - _a.y * _b.x }; }
    template<class T, int M> constexpr T dot(const vec<T, M>& _a, const vec<T, M>& _b) { return sum(_a * _b); }
    template<class T, int M> constexpr T length2(const vec<T, M>& _a) { return dot(_a, _a); }
    template<class T, int M> T           length(const vec<T, M>& _a) { return std::sqrt(length2(_a)); }
    template<class T, int M> vec<T, M>    normalize(const vec<T, M>& _a) { return _a / length(_a); }
    template<class T, int M> constexpr T distance2(const vec<T, M>& _a, const vec<T, M>& _b) { return length2(_b - _a); }
    template<class T, int M> T           distance(const vec<T, M>& _a, const vec<T, M>& _b) { return length(_b - _a); }
    template<class T, int M> T           uangle(const vec<T, M>& _a, const vec<T, M>& _b) { T d = dot(_a, _b); return d > 1 ? 0 : std::acos(d < -1 ? -1 : d); }
    template<class T, int M> T           angle(const vec<T, M>& _a, const vec<T, M>& _b) { return uangle(normalize(_a), normalize(_b)); }
    template<class T> vec<T, 2>           rot(T _a, const vec<T, 2>& _v) { const T s = std::sin(_a), c = std::cos(_a); return { _v.x * c - _v.y * s, _v.x * s + _v.y * c }; }
    template<class T, int M> vec<T, M>    nlerp(const vec<T, M>& _a, const vec<T, M>& _b, T _t) { return normalize(lerp(_a, _b, _t)); }
    template<class T, int M> vec<T, M>    slerp(const vec<T, M>& _a, const vec<T, M>& _b, T _t) { T th = uangle(_a, _b); return th == 0 ? _a : _a * (std::sin(th * (1 - _t)) / std::sin(th)) + _b * (std::sin(th * _t) / std::sin(th)); }

    // Support for quaternion algebra using 4D vectors, representing xi + yj + zk + w
    template<class T> constexpr vec<T, 4> qconj(const vec<T, 4>& _q) { return { -_q.x,-_q.y,-_q.z,_q.w }; }
    template<class T> vec<T, 4>           qinv(const vec<T, 4>& _q) { return qconj(_q) / length2(_q); }
    template<class T> vec<T, 4>           qexp(const vec<T, 4>& _q) { const auto v = _q.xyz(); const auto vv = length(v); return std::exp(_q.w) * vec<T, 4>{v* (vv > 0 ? std::sin(vv) / vv : 0), std::cos(vv)}; }
    template<class T> vec<T, 4>           qlog(const vec<T, 4>& _q) { const auto v = _q.xyz(); const auto vv = length(v), qq = length(_q); return { v * (vv > 0 ? std::acos(_q.w / qq) / vv : 0), std::log(qq) }; }
    template<class T> vec<T, 4>           qpow(const vec<T, 4>& _q, const T& _p) { const auto v = _q.xyz(); const auto vv = length(v), qq = length(_q), th = std::acos(_q.w / qq); return std::pow(qq, _p) * vec<T, 4>{v* (vv > 0 ? std::sin(_p * th) / vv : 0), std::cos(_p* th)}; }
    template<class T> constexpr vec<T, 4> qmul(const vec<T, 4>& _a, const vec<T, 4>& _b) { return { _a.x * _b.w + _a.w * _b.x + _a.y * _b.z - _a.z * _b.y, _a.y * _b.w + _a.w * _b.y + _a.z * _b.x - _a.x * _b.z, _a.z * _b.w + _a.w * _b.z + _a.x * _b.y - _a.y * _b.x, _a.w * _b.w - _a.x * _b.x - _a.y * _b.y - _a.z * _b.z }; }
    template<class T, class... R> constexpr vec<T, 4> qmul(const vec<T, 4>& _a, R... _r) { return qmul(_a, qmul(_r...)); }

    // Support for 3D spatial rotations using quaternions, via qmul(qmul(q, v), qconj(q))
    template<class T> constexpr vec<T, 3>   qxdir(const vec<T, 4>& _q) { return { _q.w * _q.w + _q.x * _q.x - _q.y * _q.y - _q.z * _q.z, (_q.x * _q.y + _q.z * _q.w) * 2, (_q.z * _q.x - _q.y * _q.w) * 2 }; }
    template<class T> constexpr vec<T, 3>   qydir(const vec<T, 4>& _q) { return { (_q.x * _q.y - _q.z * _q.w) * 2, _q.w * _q.w - _q.x * _q.x + _q.y * _q.y - _q.z * _q.z, (_q.y * _q.z + _q.x * _q.w) * 2 }; }
    template<class T> constexpr vec<T, 3>   qzdir(const vec<T, 4>& _q) { return { (_q.z * _q.x + _q.y * _q.w) * 2, (_q.y * _q.z - _q.x * _q.w) * 2, _q.w * _q.w - _q.x * _q.x - _q.y * _q.y + _q.z * _q.z }; }
    template<class T> constexpr mat<T, 3, 3> qmat(const vec<T, 4>& _q) { return { qxdir(_q), qydir(_q), qzdir(_q) }; }
    template<class T> constexpr vec<T, 3>   qrot(const vec<T, 4>& _q, const vec<T, 3>& _v) { return qxdir(_q) * _v.x + qydir(_q) * _v.y + qzdir(_q) * _v.z; }
    template<class T> T                     qangle(const vec<T, 4>& _q) { return std::atan2(length(_q.xyz()), _q.w) * 2; }
    template<class T> vec<T, 3>             qaxis(const vec<T, 4>& _q) { return normalize(_q.xyz()); }
    template<class T> vec<T, 4>             qnlerp(const vec<T, 4>& _a, const vec<T, 4>& _b, T _t) { return nlerp(_a, dot(_a, _b) < 0 ? -_b : _b, _t); }
    template<class T> vec<T, 4>             qslerp(const vec<T, 4>& _a, const vec<T, 4>& _b, T _t) { return slerp(_a, dot(_a, _b) < 0 ? -_b : _b, _t); }

    // Support for matrix algebra
    template<class T, int M> constexpr vec<T, M> mul(const mat<T, M, 1>& _a, const vec<T, 1>& _b) { return _a.x * _b.x; }
    template<class T, int M> constexpr vec<T, M> mul(const mat<T, M, 2>& _a, const vec<T, 2>& _b) { return _a.x * _b.x + _a.y * _b.y; }
    template<class T, int M> constexpr vec<T, M> mul(const mat<T, M, 3>& _a, const vec<T, 3>& _b) { return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z; }
    template<class T, int M> constexpr vec<T, M> mul(const mat<T, M, 4>& _a, const vec<T, 4>& _b) { return _a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w; }
    template<class T, int M, int N> constexpr mat<T, M, 1> mul(const mat<T, M, N>& _a, const mat<T, N, 1>& _b) { return { mul(_a, _b.x) }; }
    template<class T, int M, int N> constexpr mat<T, M, 2> mul(const mat<T, M, N>& _a, const mat<T, N, 2>& _b) { return { mul(_a, _b.x), mul(_a, _b.y) }; }
    template<class T, int M, int N> constexpr mat<T, M, 3> mul(const mat<T, M, N>& _a, const mat<T, N, 3>& _b) { return { mul(_a, _b.x), mul(_a, _b.y), mul(_a, _b.z) }; }
    template<class T, int M, int N> constexpr mat<T, M, 4> mul(const mat<T, M, N>& _a, const mat<T, N, 4>& _b) { return { mul(_a, _b.x), mul(_a, _b.y), mul(_a, _b.z), mul(_a, _b.w) }; }
    template<class T, int M, int N, int P> constexpr vec<T, M> mul(const mat<T, M, N>& _a, const mat<T, N, P>& _b, const vec<T, P>& _c) { return mul(mul(_a, _b), _c); }
    template<class T, int M, int N, int P, int Q> constexpr mat<T, M, Q> mul(const mat<T, M, N>& _a, const mat<T, N, P>& _b, const mat<T, P, Q>& _c) { return mul(mul(_a, _b), _c); }
    template<class T, int M, int N, int P, int Q> constexpr vec<T, M> mul(const mat<T, M, N>& _a, const mat<T, N, P>& _b, const mat<T, P, Q>& _c, const vec<T, Q>& _d) { return mul(mul(_a, _b, _c), _d); }
    template<class T, int M, int N, int P, int Q, int R> constexpr mat<T, M, R> mul(const mat<T, M, N>& _a, const mat<T, N, P>& _b, const mat<T, P, Q>& _c, const mat<T, Q, R>& _d) { return mul(mul(_a, _b, _c), _d); }
    // TODO: Variadic version of mul(...) that works on all compilers
    template<class T, int M> constexpr mat<T, M, 1> outerprod(const vec<T, M>& _a, const vec<T, 1>& _b) { return { _a * _b.x }; }
    template<class T, int M> constexpr mat<T, M, 2> outerprod(const vec<T, M>& _a, const vec<T, 2>& _b) { return { _a * _b.x, _a * _b.y }; }
    template<class T, int M> constexpr mat<T, M, 3> outerprod(const vec<T, M>& _a, const vec<T, 3>& _b) { return { _a * _b.x, _a * _b.y, _a * _b.z }; }
    template<class T, int M> constexpr mat<T, M, 4> outerprod(const vec<T, M>& _a, const vec<T, 4>& _b) { return { _a * _b.x, _a * _b.y, _a * _b.z, _a * _b.w }; }
    template<class T> constexpr vec<T, 1> diagonal(const mat<T, 1, 1>& _a) { return { _a.x.x }; }
    template<class T> constexpr vec<T, 2> diagonal(const mat<T, 2, 2>& _a) { return { _a.x.x, _a.y.y }; }
    template<class T> constexpr vec<T, 3> diagonal(const mat<T, 3, 3>& _a) { return { _a.x.x, _a.y.y, _a.z.z }; }
    template<class T> constexpr vec<T, 4> diagonal(const mat<T, 4, 4>& _a) { return { _a.x.x, _a.y.y, _a.z.z, _a.w.w }; }
    template<class T, int N> constexpr T trace(const mat<T, N, N>& _a) { return sum(diagonal(_a)); }
    template<class T, int M> constexpr mat<T, M, 1> transpose(const mat<T, 1, M>& _m) { return { _m.row(0) }; }
    template<class T, int M> constexpr mat<T, M, 2> transpose(const mat<T, 2, M>& _m) { return { _m.row(0), _m.row(1) }; }
    template<class T, int M> constexpr mat<T, M, 3> transpose(const mat<T, 3, M>& _m) { return { _m.row(0), _m.row(1), _m.row(2) }; }
    template<class T, int M> constexpr mat<T, M, 4> transpose(const mat<T, 4, M>& _m) { return { _m.row(0), _m.row(1), _m.row(2), _m.row(3) }; }
    template<class T> constexpr mat<T, 1, 1> adjugate(const mat<T, 1, 1>& _a) { return { vec<T,1>{1} }; }
    template<class T> constexpr mat<T, 2, 2> adjugate(const mat<T, 2, 2>& _a) { return { {_a.y.y, -_a.x.y}, {-_a.y.x, _a.x.x} }; }
    template<class T> constexpr mat<T, 3, 3> adjugate(const mat<T, 3, 3>& _a);
    template<class T> constexpr mat<T, 4, 4> adjugate(const mat<T, 4, 4>& _a);
    template<class T, int N> constexpr mat<T, N, N> comatrix(const mat<T, N, N>& _a) { return transpose(adjugate(_a)); }
    template<class T> constexpr T determinant(const mat<T, 1, 1>& _a) { return _a.x.x; }
    template<class T> constexpr T determinant(const mat<T, 2, 2>& _a) { return _a.x.x * _a.y.y - _a.x.y * _a.y.x; }
    template<class T> constexpr T determinant(const mat<T, 3, 3>& _a) { return _a.x.x * (_a.y.y * _a.z.z - _a.z.y * _a.y.z) + _a.x.y * (_a.y.z * _a.z.x - _a.z.z * _a.y.x) + _a.x.z * (_a.y.x * _a.z.y - _a.z.x * _a.y.y); }
    template<class T> constexpr T determinant(const mat<T, 4, 4>& _a);
    template<class T, int N> constexpr mat<T, N, N> inverse(const mat<T, N, N>& _a) { return adjugate(_a) / determinant(_a); }

    // Vectors and matrices can be used as ranges
    template<class T, int M>       T* begin(vec<T, M>& _a) { return &_a.x; }
    template<class T, int M> const T* begin(const vec<T, M>& _a) { return &_a.x; }
    template<class T, int M>       T* end(vec<T, M>& _a) { return begin(_a) + M; }
    template<class T, int M> const T* end(const vec<T, M>& _a) { return begin(_a) + M; }
    template<class T, int M, int N>       vec<T, M>* begin(mat<T, M, N>& _a) { return &_a.x; }
    template<class T, int M, int N> const vec<T, M>* begin(const mat<T, M, N>& _a) { return &_a.x; }
    template<class T, int M, int N>       vec<T, M>* end(mat<T, M, N>& _a) { return begin(_a) + N; }
    template<class T, int M, int N> const vec<T, M>* end(const mat<T, M, N>& _a) { return begin(_a) + N; }

    // Factory functions for 3D spatial transformations (will possibly be removed or changed in a future version)
    enum fwd_axis { neg_z, pos_z };                 // Should projection matrices be generated assuming forward is {0,0,-1} or {0,0,1}
    enum z_range { neg_one_to_one, zero_to_one };   // Should projection matrices map z into the range of [-1,1] or [0,1]?
    template<class T> vec<T, 4>   rotation_quat(const vec<T, 3>& _axis, T _angle) { return { _axis * std::sin(_angle / 2), std::cos(_angle / 2) }; }
    template<class T> vec<T, 4>   rotation_quat(const mat<T, 3, 3>& _m);
    template<class T> mat<T, 4, 4> translation_matrix(const vec<T, 3>& _translation) { return { {1,0,0,0},{0,1,0,0},{0,0,1,0},{_translation,1} }; }
    template<class T> mat<T, 4, 4> rotation_matrix(const vec<T, 4>& _rotation) { return { {qxdir(_rotation),0}, {qydir(_rotation),0}, {qzdir(_rotation),0}, {0,0,0,1} }; }
    template<class T> mat<T, 4, 4> scaling_matrix(const vec<T, 3>& _scaling) { return { {_scaling.x,0,0,0}, {0,_scaling.y,0,0}, {0,0,_scaling.z,0}, {0,0,0,1} }; }
    template<class T> mat<T, 4, 4> pose_matrix(const vec<T, 4>& _q, const vec<T, 3>& _p) { return { {qxdir(_q),0}, {qydir(_q),0}, {qzdir(_q),0}, {_p,1} }; }
    template<class T> mat<T, 4, 4> frustum_matrix(T _x0, T _x1, T _y0, T _y1, T _n, T _f, fwd_axis _a = neg_z, z_range _z = neg_one_to_one);
    template<class T> mat<T, 4, 4> perspective_matrix(T _fovy, T _aspect, T _n, T _f, fwd_axis _a = neg_z, z_range _z = neg_one_to_one) { T y = n * std::tan(_fovy / 2), x = y * _aspect; return frustum_matrix(-x, x, -y, y, _n, _f, _a, _z); }
    template<class T> mat<T, 4, 4> view_matrix(const vec<T, 3>& _eye, const vec<T, 3>& _target, const vec<T, 3>& _up);

    // Provide implicit conversion between linalg::vec<T,M> and std::array<T,M>
    template<class T> struct converter<vec<T, 1>, std::array<T, 1>> { vec<T, 1> operator() (const std::array<T, 1>& _a) const { return { _a[0] }; } };
    template<class T> struct converter<vec<T, 2>, std::array<T, 2>> { vec<T, 2> operator() (const std::array<T, 2>& _a) const { return { _a[0], _a[1] }; } };
    template<class T> struct converter<vec<T, 3>, std::array<T, 3>> { vec<T, 3> operator() (const std::array<T, 3>& _a) const { return { _a[0], _a[1], _a[2] }; } };
    template<class T> struct converter<vec<T, 4>, std::array<T, 4>> { vec<T, 4> operator() (const std::array<T, 4>& _a) const { return { _a[0], _a[1], _a[2], _a[3] }; } };

    template<class T> struct converter<std::array<T, 1>, vec<T, 1>> { std::array<T, 1> operator() (const vec<T, 1>& _a) const { return { _a[0] }; } };
    template<class T> struct converter<std::array<T, 2>, vec<T, 2>> { std::array<T, 2> operator() (const vec<T, 2>& _a) const { return { _a[0], _a[1] }; } };
    template<class T> struct converter<std::array<T, 3>, vec<T, 3>> { std::array<T, 3> operator() (const vec<T, 3>& _a) const { return { _a[0], _a[1], _a[2] }; } };
    template<class T> struct converter<std::array<T, 4>, vec<T, 4>> { std::array<T, 4> operator() (const vec<T, 4>& _a) const { return { _a[0], _a[1], _a[2], _a[3] }; } };

    // Provide typedefs for common element types and vector/matrix sizes
    namespace aliases
    {
        typedef vec<bool, 1> bool1; typedef vec<uint8_t, 1> byte1; typedef vec<int16_t, 1> short1; typedef vec<uint16_t, 1> ushort1;
        typedef vec<bool, 2> bool2; typedef vec<uint8_t, 2> byte2; typedef vec<int16_t, 2> short2; typedef vec<uint16_t, 2> ushort2;
        typedef vec<bool, 3> bool3; typedef vec<uint8_t, 3> byte3; typedef vec<int16_t, 3> short3; typedef vec<uint16_t, 3> ushort3;
        typedef vec<bool, 4> bool4; typedef vec<uint8_t, 4> byte4; typedef vec<int16_t, 4> short4; typedef vec<uint16_t, 4> ushort4;
        typedef vec<int, 1> int1; typedef vec<unsigned, 1> uint1; typedef vec<float, 1> float1; typedef vec<double, 1> double1;
        typedef vec<int, 2> int2; typedef vec<unsigned, 2> uint2; typedef vec<float, 2> float2; typedef vec<double, 2> double2;
        typedef vec<int, 3> int3; typedef vec<unsigned, 3> uint3; typedef vec<float, 3> float3; typedef vec<double, 3> double3;
        typedef vec<int, 4> int4; typedef vec<unsigned, 4> uint4; typedef vec<float, 4> float4; typedef vec<double, 4> double4;
        typedef mat<bool, 1, 1> bool1x1; typedef mat<int, 1, 1> int1x1; typedef mat<float, 1, 1> float1x1; typedef mat<double, 1, 1> double1x1;
        typedef mat<bool, 1, 2> bool1x2; typedef mat<int, 1, 2> int1x2; typedef mat<float, 1, 2> float1x2; typedef mat<double, 1, 2> double1x2;
        typedef mat<bool, 1, 3> bool1x3; typedef mat<int, 1, 3> int1x3; typedef mat<float, 1, 3> float1x3; typedef mat<double, 1, 3> double1x3;
        typedef mat<bool, 1, 4> bool1x4; typedef mat<int, 1, 4> int1x4; typedef mat<float, 1, 4> float1x4; typedef mat<double, 1, 4> double1x4;
        typedef mat<bool, 2, 1> bool2x1; typedef mat<int, 2, 1> int2x1; typedef mat<float, 2, 1> float2x1; typedef mat<double, 2, 1> double2x1;
        typedef mat<bool, 2, 2> bool2x2; typedef mat<int, 2, 2> int2x2; typedef mat<float, 2, 2> float2x2; typedef mat<double, 2, 2> double2x2;
        typedef mat<bool, 2, 3> bool2x3; typedef mat<int, 2, 3> int2x3; typedef mat<float, 2, 3> float2x3; typedef mat<double, 2, 3> double2x3;
        typedef mat<bool, 2, 4> bool2x4; typedef mat<int, 2, 4> int2x4; typedef mat<float, 2, 4> float2x4; typedef mat<double, 2, 4> double2x4;
        typedef mat<bool, 3, 1> bool3x1; typedef mat<int, 3, 1> int3x1; typedef mat<float, 3, 1> float3x1; typedef mat<double, 3, 1> double3x1;
        typedef mat<bool, 3, 2> bool3x2; typedef mat<int, 3, 2> int3x2; typedef mat<float, 3, 2> float3x2; typedef mat<double, 3, 2> double3x2;
        typedef mat<bool, 3, 3> bool3x3; typedef mat<int, 3, 3> int3x3; typedef mat<float, 3, 3> float3x3; typedef mat<double, 3, 3> double3x3;
        typedef mat<bool, 3, 4> bool3x4; typedef mat<int, 3, 4> int3x4; typedef mat<float, 3, 4> float3x4; typedef mat<double, 3, 4> double3x4;
        typedef mat<bool, 4, 1> bool4x1; typedef mat<int, 4, 1> int4x1; typedef mat<float, 4, 1> float4x1; typedef mat<double, 4, 1> double4x1;
        typedef mat<bool, 4, 2> bool4x2; typedef mat<int, 4, 2> int4x2; typedef mat<float, 4, 2> float4x2; typedef mat<double, 4, 2> double4x2;
        typedef mat<bool, 4, 3> bool4x3; typedef mat<int, 4, 3> int4x3; typedef mat<float, 4, 3> float4x3; typedef mat<double, 4, 3> double4x3;
        typedef mat<bool, 4, 4> bool4x4; typedef mat<int, 4, 4> int4x4; typedef mat<float, 4, 4> float4x4; typedef mat<double, 4, 4> double4x4;
    }

    // Provide output streaming operators, writing something that resembles an aggregate literal that could be used to construct the specified value
    namespace ostream_overloads
    {
        template<class C, class T> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const vec<T, 1>& _v) { return _out << '{' << _v[0] << '}'; }
        template<class C, class T> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const vec<T, 2>& _v) { return _out << '{' << _v[0] << ',' << _v[1] << '}'; }
        template<class C, class T> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const vec<T, 3>& _v) { return _out << '{' << _v[0] << ',' << _v[1] << ',' << _v[2] << '}'; }
        template<class C, class T> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const vec<T, 4>& _v) { return _out << '{' << _v[0] << ',' << _v[1] << ',' << _v[2] << ',' << _v[3] << '}'; }

        template<class C, class T, int M> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const mat<T, M, 1>& _m) { return _out << '{' << _m[0] << '}'; }
        template<class C, class T, int M> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const mat<T, M, 2>& _m) { return _out << '{' << _m[0] << ',' << _m[1] << '}'; }
        template<class C, class T, int M> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const mat<T, M, 3>& _m) { return _out << '{' << _m[0] << ',' << _m[1] << ',' << _m[2] << '}'; }
        template<class C, class T, int M> std::basic_ostream<C>& operator << (std::basic_ostream<C>& _out, const mat<T, M, 4>& _m) { return _out << '{' << _m[0] << ',' << _m[1] << ',' << _m[2] << ',' << _m[3] << '}'; }
    }
}

namespace std
{
    // Provide specializations for std::hash<...> with linalg types
    template<class T> struct hash<linalg::vec<T, 1>> { std::size_t operator()(const linalg::vec<T, 1>& _v) const { std::hash<T> h; return h(_v.x); } };
    template<class T> struct hash<linalg::vec<T, 2>> { std::size_t operator()(const linalg::vec<T, 2>& _v) const { std::hash<T> h; return h(_v.x) ^ (h(_v.y) << 1); } };
    template<class T> struct hash<linalg::vec<T, 3>> { std::size_t operator()(const linalg::vec<T, 3>& _v) const { std::hash<T> h; return h(_v.x) ^ (h(_v.y) << 1) ^ (h(_v.z) << 2); } };
    template<class T> struct hash<linalg::vec<T, 4>> { std::size_t operator()(const linalg::vec<T, 4>& _v) const { std::hash<T> h; return h(_v.x) ^ (h(_v.y) << 1) ^ (h(_v.z) << 2) ^ (h(_v.w) << 3); } };

    template<class T, int M> struct hash<linalg::mat<T, M, 1>> { std::size_t operator()(const linalg::mat<T, M, 1>& _v) const { std::hash<linalg::vec<T, M>> h; return h(_v.x); } };
    template<class T, int M> struct hash<linalg::mat<T, M, 2>> { std::size_t operator()(const linalg::mat<T, M, 2>& _v) const { std::hash<linalg::vec<T, M>> h; return h(_v.x) ^ (h(_v.y) << M); } };
    template<class T, int M> struct hash<linalg::mat<T, M, 3>> { std::size_t operator()(const linalg::mat<T, M, 3>& _v) const { std::hash<linalg::vec<T, M>> h; return h(_v.x) ^ (h(_v.y) << M) ^ (h(_v.z) << (M * 2)); } };
    template<class T, int M> struct hash<linalg::mat<T, M, 4>> { std::size_t operator()(const linalg::mat<T, M, 4>& _v) const { std::hash<linalg::vec<T, M>> h; return h(_v.x) ^ (h(_v.y) << M) ^ (h(_v.z) << (M * 2)) ^ (h(_v.w) << (M * 3)); } };
}

// Definitions of functions too long to be defined inline
template<class T> constexpr linalg::mat<T, 3, 3> linalg::adjugate(const mat<T, 3, 3>& a)
{
    return { {_a.y.y * _a.z.z - _a.z.y * _a.y.z, _a.z.y * _a.x.z - _a.x.y * _a.z.z, _a.x.y * _a.y.z - _a.y.y * _a.x.z},
            {_a.y.z * _a.z.x - _a.z.z * _a.y.x, _a.z.z * _a.x.x - _a.x.z * _a.z.x, _a.x.z * _a.y.x - _a.y.z * _a.x.x},
            {_a.y.x * _a.z.y - _a.z.x * _a.y.y, _a.z.x * _a.x.y - _a.x.x * _a.z.y, _a.x.x * _a.y.y - _a.y.x * _a.x.y} };
}

template<class T> constexpr linalg::mat<T, 4, 4> linalg::adjugate(const mat<T, 4, 4>& _a)
{
    return { {_a.y.y * _a.z.z * _a.w.w + _a.w.y * _a.y.z * _a.z.w + _a.z.y * _a.w.z * _a.y.w - _a.y.y * _a.w.z * _a.z.w - _a.z.y * _a.y.z * _a.w.w - _a.w.y * _a.z.z * _a.y.w,
             _a.x.y * _a.w.z * _a.z.w + _a.z.y * _a.x.z * _a.w.w + _a.w.y * _a.z.z * _a.x.w - _a.w.y * _a.x.z * _a.z.w - _a.z.y * _a.w.z * _a.x.w - _a.x.y * _a.z.z * _a.w.w,
             _a.x.y * _a.y.z * _a.w.w + _a.w.y * _a.x.z * _a.y.w + _a.y.y * _a.w.z * _a.x.w - _a.x.y * _a.w.z * _a.y.w - _a.y.y * _a.x.z * _a.w.w - _a.w.y * _a.y.z * _a.x.w,
             _a.x.y * _a.z.z * _a.y.w + _a.y.y * _a.x.z * _a.z.w + _a.z.y * _a.y.z * _a.x.w - _a.x.y * _a.y.z * _a.z.w - _a.z.y * _a.x.z * _a.y.w - _a.y.y * _a.z.z * _a.x.w},
            {_a.y.z * _a.w.w * _a.z.x + _a.z.z * _a.y.w * _a.w.x + _a.w.z * _a.z.w * _a.y.x - _a.y.z * _a.z.w * _a.w.x - _a.w.z * _a.y.w * _a.z.x - _a.z.z * _a.w.w * _a.y.x,
             _a.x.z * _a.z.w * _a.w.x + _a.w.z * _a.x.w * _a.z.x + _a.z.z * _a.w.w * _a.x.x - _a.x.z * _a.w.w * _a.z.x - _a.z.z * _a.x.w * _a.w.x - _a.w.z * _a.z.w * _a.x.x,
             _a.x.z * _a.w.w * _a.y.x + _a.y.z * _a.x.w * _a.w.x + _a.w.z * _a.y.w * _a.x.x - _a.x.z * _a.y.w * _a.w.x - _a.w.z * _a.x.w * _a.y.x - _a.y.z * _a.w.w * _a.x.x,
             _a.x.z * _a.y.w * _a.z.x + _a.z.z * _a.x.w * _a.y.x + _a.y.z * _a.z.w * _a.x.x - _a.x.z * _a.z.w * _a.y.x - _a.y.z * _a.x.w * _a.z.x - _a.z.z * _a.y.w * _a.x.x},
            {_a.y.w * _a.z.x * _a.w.y + _a.w.w * _a.y.x * _a.z.y + _a.z.w * _a.w.x * _a.y.y - _a.y.w * _a.w.x * _a.z.y - _a.z.w * _a.y.x * _a.w.y - _a.w.w * _a.z.x * _a.y.y,
             _a.x.w * _a.w.x * _a.z.y + _a.z.w * _a.x.x * _a.w.y + _a.w.w * _a.z.x * _a.x.y - _a.x.w * _a.z.x * _a.w.y - _a.w.w * _a.x.x * _a.z.y - _a.z.w * _a.w.x * _a.x.y,
             _a.x.w * _a.y.x * _a.w.y + _a.w.w * _a.x.x * _a.y.y + _a.y.w * _a.w.x * _a.x.y - _a.x.w * _a.w.x * _a.y.y - _a.y.w * _a.x.x * _a.w.y - _a.w.w * _a.y.x * _a.x.y,
             _a.x.w * _a.z.x * _a.y.y + _a.y.w * _a.x.x * _a.z.y + _a.z.w * _a.y.x * _a.x.y - _a.x.w * _a.y.x * _a.z.y - _a.z.w * _a.x.x * _a.y.y - _a.y.w * _a.z.x * _a.x.y},
            {_a.y.x * _a.w.y * _a.z.z + _a.z.x * _a.y.y * _a.w.z + _a.w.x * _a.z.y * _a.y.z - _a.y.x * _a.z.y * _a.w.z - _a.w.x * _a.y.y * _a.z.z - _a.z.x * _a.w.y * _a.y.z,
             _a.x.x * _a.z.y * _a.w.z + _a.w.x * _a.x.y * _a.z.z + _a.z.x * _a.w.y * _a.x.z - _a.x.x * _a.w.y * _a.z.z - _a.z.x * _a.x.y * _a.w.z - _a.w.x * _a.z.y * _a.x.z,
             _a.x.x * _a.w.y * _a.y.z + _a.y.x * _a.x.y * _a.w.z + _a.w.x * _a.y.y * _a.x.z - _a.x.x * _a.y.y * _a.w.z - _a.w.x * _a.x.y * _a.y.z - _a.y.x * _a.w.y * _a.x.z,
             _a.x.x * _a.y.y * _a.z.z + _a.z.x * _a.x.y * _a.y.z + _a.y.x * _a.z.y * _a.x.z - _a.x.x * _a.z.y * _a.y.z - _a.y.x * _a.x.y * _a.z.z - _a.z.x * _a.y.y * _a.x.z} };
}

template<class T> constexpr T linalg::determinant(const mat<T, 4, 4>& _a)
{
    return _a.x.x * (_a.y.y * _a.z.z * _a.w.w + _a.w.y * _a.y.z * _a.z.w + _a.z.y * _a.w.z * _a.y.w - _a.y.y * _a.w.z * _a.z.w - _a.z.y * _a.y.z * _a.w.w - _a.w.y * _a.z.z * _a.y.w)
        + _a.x.y * (_a.y.z * _a.w.w * _a.z.x + _a.z.z * _a.y.w * _a.w.x + _a.w.z * _a.z.w * _a.y.x - _a.y.z * _a.z.w * _a.w.x - _a.w.z * _a.y.w * _a.z.x - _a.z.z * _a.w.w * _a.y.x)
        + _a.x.z * (_a.y.w * _a.z.x * _a.w.y + _a.w.w * _a.y.x * _a.z.y + _a.z.w * _a.w.x * _a.y.y - _a.y.w * _a.w.x * _a.z.y - _a.z.w * _a.y.x * _a.w.y - _a.w.w * _a.z.x * _a.y.y)
        + _a.x.w * (_a.y.x * _a.w.y * _a.z.z + _a.z.x * _a.y.y * _a.w.z + _a.w.x * _a.z.y * _a.y.z - _a.y.x * _a.z.y * _a.w.z - _a.w.x * _a.y.y * _a.z.z - _a.z.x * _a.w.y * _a.y.z);
}

template<class T> linalg::vec<T, 4> linalg::rotation_quat(const mat<T, 3, 3>& _m)
{
    const vec<T, 4> q{ _m.x.x - _m.y.y - _m.z.z, _m.y.y - _m.x.x - _m.z.z, _m.z.z - _m.x.x - _m.y.y, _m.x.x + _m.y.y + _m.z.z }, s[]{
        {1, _m.x.y + _m.y.x, _m.z.x + _m.x.z, _m.y.z - _m.z.y},
        {_m.x.y + _m.y.x, 1, _m.y.z + _m.z.y, _m.z.x - _m.x.z},
        {_m.x.z + _m.z.x, _m.y.z + _m.z.y, 1, _m.x.y - _m.y.x},
        {_m.y.z - _m.z.y, _m.z.x - _m.x.z, _m.x.y - _m.y.x, 1} };
    return copysign(normalize(sqrt(max(T(0), T(1) + q))), s[argmax(q)]);
}

template<class T> linalg::mat<T, 4, 4> linalg::frustum_matrix(T _x0, T _x1, T _y0, T _y1, T _n, T _f, fwd_axis _a, z_range _z)
{
    const T s = _a == pos_z ? T(1) : T(-1), o = _z == neg_one_to_one ? _n : 0;
    return { {2 * _n / (_x1 - _x0),0,0,0}, {0,2 * _n / (_y1 - _y0),0,0}, {-s * (_x0 + _x1) / (_x1 - _x0),-s * (_y0 + _y1) / (_y1 - _y0),s * (_f + o) / (_f - _n),s}, {0,0,-(_n + o) * _f / (_f - _n),0} };
}

template<class T> linalg::mat<T, 4, 4> linalg::view_matrix(const vec<T, 3>& _eye, const vec<T, 3>& _target, const vec<T, 3>& _up)
{
    vec<T, 3> zaxis = normalize(_eye - _target);    // The "forward" vector.
    vec<T, 3> xaxis = normalize(cross(_up, zaxis));// The "right" vector.
    vec<T, 3> yaxis = cross(zaxis, xaxis);     // The "up" vector.

    return {
       vec<T, 4>(xaxis.x, yaxis.x, zaxis.x, 0),
       vec<T, 4>(xaxis.y, yaxis.y, zaxis.y, 0),
       vec<T, 4>(xaxis.z, yaxis.z, zaxis.z, 0),
       vec<T, 4>(-dot(xaxis, _eye), -dot(yaxis, _eye), -dot(zaxis, _eye), 1)
    };
}

#endif
