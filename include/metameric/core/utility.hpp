#pragma once

#include <metameric/core/detail/trace.hpp>
#include <metameric/core/detail/utility.hpp>
#include <ranges>
#include <span>
#include <source_location>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

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
  // Namespace shorthands
  namespace rng = std::ranges;
  namespace vws = std::views;

  // Interpret a container type as a span of type T
  template <class T, class C>
  constexpr
  std::span<T> cnt_span(C &c) {
    auto data = c.data();
    guard(data, {});
    return { reinterpret_cast<T*>(data), (c.size() * sizeof(C::value_type)) / sizeof(T) };
  }

  template <class T, class O>
  constexpr
  std::span<T> obj_span(O &o) {
    return { reinterpret_cast<T*>(&o), sizeof(O) / sizeof(T) };
  }

  // Convert a span over type U to 
  template <class T, class U>
  std::span<T> cast_span(std::span<U> s) {
    auto data = s.data();
    guard(data, {});
    return { reinterpret_cast<T*>(data), s.size_bytes() / sizeof(T) };
  }

  template <typename T, typename T_>
  constexpr inline T ceil_div(T n, T_ div) {
    return (n + static_cast<T>(div) - T(1)) / static_cast<T>(div);
  }

  namespace debug {
    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    // Note: can be removed on release builds
  #if defined(NDEBUG) || defined(MET_ENABLE_EXCEPTIONS)
    constexpr inline
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
} // namespace met