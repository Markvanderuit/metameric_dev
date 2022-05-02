#pragma once

#include <exception>
#include <map>
#include <string>
#include <source_location>
#include <fmt/format.h>
#include <metameric/core/define.h>
#include <metameric/core/fwd.h>

namespace metameric {
  namespace detail {
    class RuntimeException : public std::exception {
      mutable std::string _what;
      std::string _msg;
      std::map<std::string, std::string> _attached;

    public:
      RuntimeException() = default;
      RuntimeException(const std::string &msg) : _msg(msg) { }

      const char * what() const noexcept override {
        constexpr std::string_view fmt = "- {:<7} : {}\n";
        std::string s = "Runtime exception\n";

        if (!_msg.empty()) {
          s += fmt::format(fmt, "message", _msg);
        }

        for (const auto &[key, msg] : _attached)
          s += fmt::format(fmt, key, msg);

        return (_what = s).c_str();
      }

      std::string& operator[](const std::string &key) {
        return _attached[key];
      }
    };
  } // namespace detail
  
  inline
  void runtime_assert(bool expr, 
                      const std::string &msg = "",
                      const std::source_location loc = std::source_location::current()) {
    guard(!expr);

    detail::RuntimeException e(msg);
    e["src"]  = "metameric::runtime_assert";
    e["file"]  = fmt::format("{}({}:{})", loc.file_name(), loc.line(), loc.column());
    e["func"] = loc.function_name();
    throw e;
  }
} // namespace metameric