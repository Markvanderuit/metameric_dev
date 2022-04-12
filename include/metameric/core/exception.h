#pragma once

#include <exception>
#include <map>
#include <string>
#include <fmt/core.h>
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

        s += fmt::format(fmt, "message", _msg);
        for (const auto &[key, msg] : _attached)
          s += fmt::format(fmt, key, msg);

        return (_what = s).c_str();
      }

      std::string& operator[](const std::string &key) {
        return _attached[key];
      }
    };
  
    inline
    void runtime_assert_impl(bool expr, const std::string &msg, const char *file_path, uint line_nr) {
      guard(!expr);

      RuntimeException e(msg);
      e["file_path"] = file_path;
      e["line_nr"] = std::to_string(line_nr);
      
      throw e;
    }
  } // namespace detail
} // namespace metameric

#define runtime_assert(expr, msg)\
  metameric::detail::runtime_assert_impl(expr, msg, __FILE__, __LINE__);