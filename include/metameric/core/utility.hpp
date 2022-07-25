#pragma once

#include <metameric/core/detail/utility.hpp>
#include <source_location>
#include <span>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

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
    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    constexpr inline
    void check_expr(bool expr,
                    const std::string_view &msg = "",
                    const std::source_location sl = std::source_location::current()) {
  #ifdef NDEBUG
  #else
      guard(!expr);

      detail::Exception e;
      e.put("src", "met::debug::check_expr(...) failed, checked expression evaluated to false");
      e.put("message", msg);
      e.put("in file", fmt::format("{}({}:{})", sl.file_name(), sl.line(), sl.column()));
      throw e;
  #endif
    }
  } // namespace debug
} // namespace met