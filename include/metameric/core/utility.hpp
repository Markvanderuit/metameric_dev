#pragma once

#include <metameric/core/fwd.hpp>
#include <metameric/core/detail/utility.hpp>
#include <source_location>

// Simple guard statement syntactic sugar
#define guard(expr,...) if (!(expr)) { return __VA_ARGS__ ; }
#define guard_continue(expr) if (!(expr)) { continue; }
#define guard_break(expr) if (!(expr)) { break; }

namespace met {
  namespace debug {
    // Evaluate a boolean expression, throwing a detailed exception pointing
    // to the expression's origin if said expression fails
    inline
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