#pragma once

#define MET_TRACY_ENABLED // Settings for now
#ifdef  MET_TRACY_ENABLED
#include <small_gl/utility.hpp>
#include <Tracy.hpp>
#include <TracyOpenGL.hpp>
#endif // MET_TRACY_ENABLED

#include <metameric/core/detail/utility.hpp>
#include <span>
#include <source_location>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

// Simple range-like syntactic sugar
#define range_iter(c) c.begin(), c.end()

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

// Insert Tracy scope statements if tracing is enabled
#ifndef MET_TRACY_ENABLED
  #define met_declare_trace_text(str_ptr)
  #define met_declare_trace_zone(name_ptr)
  #define met_declare_trace_zone()
  #define met_declare_trace_frame()
#else // MET_TRACY_ENABLED
  #define met_declare_trace_text(str)      ZoneText(str, std::char_traits<char>::length(str))
  #define met_declare_trace_named(name)    ZoneScopedN(name)
  #define met_declare_trace_zone()         ZoneScoped
  #define met_declare_trace_zone_gpu(name) TracyGpuZone(name)      
  #define met_declare_trace_frame()        TracyGpuCollect; FrameMark;

  #ifndef TRACY_ENABLE
    #define TRACY_ENABLE
  #endif // TRACY_ENABLE
  #define TRACY_ON_DEMAND
#endif // MET_TRACY_ENABLED

namespace met {
  // Interpret a container type as a span of type T
  template <class T, class C>
  std::span<T> as_span(C &c) {
    auto data = c.data();
    guard(data, {});
    return { reinterpret_cast<T*>(data), (c.size() * sizeof(C::value_type)) / sizeof(T) };
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
    constexpr inline
    void check_expr_rel(bool expr,
                        const std::string_view &msg = "",
                        const std::source_location sl = std::source_location::current()) {
      guard(!expr);

      detail::Exception e;
      e.put("src", "met::debug::check_expr_rel(...) failed, checked expression evaluated to false");
      e.put("message", msg);
      e.put("in file", fmt::format("{}({}:{})", sl.file_name(), sl.line(), sl.column()));
      throw e;
    }

    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    constexpr inline
    void check_expr_dbg(bool expr,
                        const std::string_view &msg = "",
                        const std::source_location sl = std::source_location::current()) {
  #ifdef NDEBUG
  #else
      guard(!expr);

      detail::Exception e;
      e.put("src", "met::debug::check_expr_dbg(...) failed, checked expression evaluated to false");
      e.put("message", msg);
      e.put("in file", fmt::format("{}({}:{})", sl.file_name(), sl.line(), sl.column()));
      throw e;
  #endif
    }
  } // namespace debug
} // namespace met