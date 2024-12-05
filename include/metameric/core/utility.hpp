// Copyright (C) 2024 Mark van de Ruit, Delft University of Technology.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/utility.hpp>
#include <span>
#include <source_location>

// Simple guard statement syntactic sugar
#define guard(expr,...)                if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr)           if (!(expr)) { continue; }
#define guard_break(expr)              if (!(expr)) { break; }
#define guard_constexpr(expr,...)      if constexpr (!(expr)) { return __VA_ARGS__ ; }
#define guard_constexpr_continue(expr) if constexpr (!(expr)) { continue; }
#define guard_constexpr_break(expr)    if constexpr (!(expr)) { break; }

// Simple range-like syntactic sugar
#define range_iter(c)  c.begin(), c.end()
#define range_riter(c) c.rbegin(), c.rend()

// For enum class T, declare bitflag operators and has_flag(T, T) boolean operator
#define met_declare_bitflag(T)                                               \
  constexpr T operator~(T a) { return (T) (~ (uint) a); }                    \
  constexpr T operator|(T a, T b) { return (T) ((uint) a | (uint) b); }      \
  constexpr T operator&(T a, T b) { return (T) ((uint) a & (uint) b); }      \
  constexpr T operator^(T a, T b) { return (T) ((uint) a ^ (uint) b); }      \
  constexpr T& operator|=(T &a, T b) { return a = a | b; }                   \
  constexpr T& operator&=(T &a, T b) { return a = a & b; }                   \
  constexpr T& operator^=(T &a, T b) { return a = a ^ b; }                   \
  constexpr bool has_flag(T flags, T t) { return (uint) (flags & t) != 0u; }

// For class T, declare swap-based move constr/operator
// and delete copy constr/operators, making T non-copyable
#define met_declare_noncopyable(T)                                            \
  T(const T &) = delete;                                                      \
  T & operator= (const T &) = delete;                                         \
  T(T &&o) noexcept { met_trace(); swap(o); }                                 \
  inline T & operator= (T &&o) noexcept { met_trace(); swap(o); return *this; }

// Utility debug shorthands
#if defined(NDEBUG) || defined(MET_ENABLE_EXCEPTIONS)
  #define met_debug_is_enabled
  #define met_debug_insert(x)    x
  #define met_debug_select(x, y) x
  #define met_enable_debug       true
#else
  #define met_debug_insert(x)
  #define met_debug_select(x, y) y
  #define met_enable_debug       false
#endif

namespace met {
  // Interpret a sized contiguous container as a span of type T
  template <class T, class C>
  constexpr
  std::span<T> cnt_span(C &c) {
    auto data = c.data();
    guard(data, {});
    using value_type = typename C::value_type;
    return { reinterpret_cast<T*>(data), (c.size() * sizeof(value_type)) / sizeof(T) };
  }

  // Interpret an object as a span of type T
  template <class T, class O>
  constexpr
  std::span<T> obj_span(O &o) {
    return { reinterpret_cast<T*>(&o), sizeof(O) / sizeof(T) };
  }

  // Interpret a span of U to a span of type T
  template <class T, class U>
  std::span<T> cast_span(std::span<U> s) {
    auto data = s.data();
    guard(data, {});
    return { reinterpret_cast<T*>(data), s.size_bytes() / sizeof(T) };
  }
  
  // Helper; capitalize a string
  inline
  std::string to_capital(std::string s) {
    s[0] = std::toupper(s[0]);
    return s;
  }

  // Take a pair of integers, cast to same type, and do a ceiling divide
  template <typename T, typename T_>
  constexpr inline T ceil_div(T n, T_ div) {
    return (n + static_cast<T>(div) - T(1)) / static_cast<T>(div);
  }
  
  // Debug namespace; mostly check_expr(...) from here on
  namespace debug {
    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    // Note: can be removed on release builds
  #if defined(NDEBUG) || defined(MET_ENABLE_EXCEPTIONS)
    inline
    void check_expr(bool expr,
                    const std::string_view &msg = "",
                    const std::source_location sl = std::source_location::current()) {
      guard(!expr);

      detail::Exception e;
      e.put("src", "met::debug::check_expr(...) failed, checked expression evaluated to false");
      e.put("message", msg);
      e.put("in file", fmt::format("{}({}:{})", sl.file_name(), sl.line(), sl.column()));
      throw e;
    }
  #else
  #define check_expr(expr, msg, sl)
  #endif
  } // namespace debug

  // Visit a variant using syntactic sugar,
  // e.g. 'std::visit(visitor, variant)' formed from 'variant | visit { visitors... };'
  // Src: https://en.cppreference.com/w/cpp/utility/variant/visit
  // Src: https://github.com/AVasK/vx
  /*
    variant | visit {
      [](uint i)  { ... },
      [](float f) { ... },
      [](auto v)  { ... },
    }
  */
  template <typename... Ts> struct visit : Ts... { using Ts::operator()...; };
  template <typename... Ts> visit(Ts...) -> visit<Ts...>;
  template <typename... Ts, typename... Fs>
  constexpr decltype(auto) operator| (std::variant<Ts...> const& v, visit<Fs...> const& f) {
    return std::visit(f, v);
  }
  template <typename... Ts, typename... Fs>
  constexpr decltype(auto) operator| (std::variant<Ts...> & v, visit<Fs...> const& f) {
    return std::visit(f, v);
  }
  
  // Syntactic sugar for a variant | match { visitors } pattern over
  // all the known types of an std::optional, matching only if the
  // underlying type is present
  template <typename T, typename... Fs>
  requires (std::is_invocable_v<visit<Fs...>, T> && std::is_invocable_v<visit<Fs...>>)
  constexpr decltype(auto) operator| (std::optional<T> const& o, visit<Fs...> const& visit) {
    if (o.has_value()) 
      return visit(o.value());
    else 
      return visit();
  }

  // Visit a variant, s.t. the arguments passed to the capture
  // form default-initialized objects of the variant's underlying type,
  // and a boolean indicating whether the type is matched. Useful
  // for type selectors in UI components
  /* 
    variant | visit_types([](auto default_of_type, bool is_match) {
      ...
    });
   */
  template <typename T> struct visit_types : T { using T::operator(); };
  template <typename T> visit_types(T) -> visit_types<T>;
  template <typename... Ts, typename F>
  constexpr decltype(auto) operator| (std::variant<Ts...> const& v, visit_types<F> const& f) {
    using VTy = std::variant<Ts...>;
    auto indexes = detail::indexing_tuple<std::variant_size_v<VTy>>;
    return detail::tuple_visit(indexes, [&v, f](auto I) {
      using ATy = std::variant_alternative_t<I, VTy>;
      return f(ATy(), std::holds_alternative<ATy>(v));
    });
  }
  template <typename... Ts, typename F>
  constexpr decltype(auto) operator| (std::variant<Ts...> & v, visit_types<F> const& f) {
    using VTy = std::variant<Ts...>;
    auto indexes = detail::indexing_tuple<std::variant_size_v<VTy>>;
    return detail::tuple_visit(indexes, [&v, f](auto I) {
      using ATy = std::variant_alternative_t<I, VTy>;
      return f(ATy(), std::holds_alternative<ATy>(v));
    });
  }

  // Syntactic sugar for 'std::visit(visitor, variant) <- variant | visit_single(visitor);'
  /*
    variant | visit_single([](uint i)  { ... });
  */
  template <typename T> struct visit_single : T { using T::operator(); };
  template <typename T> visit_single(T) -> visit_single<T>;
  template <typename... Ts, typename F>
  constexpr void operator| (std::variant<Ts...> const& v, visit_single<F> const& f) {
    v | visit { f, [](const auto &) {} };
  }
  template <typename... Ts, typename F>
  constexpr void operator| (std::variant<Ts...> & v, visit_single<F> const& f) {
    v | visit { f, [](auto &) {} };
  }

  // Syntactic sugar to do c++20 style projection on std::tuple as tuple_project<I>
  // Src: https://stackoverflow.com/questions/64963075/how-to-project-stdtuple-in-c20-constrained-algorithms
  template<size_t n> struct tuple_project_t {
    template<typename T> decltype(auto) operator()(T&& t) const {
      using std::get;
      return get<n>(std::forward<T>(t));
    }
  };
  template<size_t n> inline constexpr tuple_project_t<n> tuple_project;
} // namespace met