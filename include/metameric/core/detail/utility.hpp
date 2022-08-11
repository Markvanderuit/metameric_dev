#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/compile.h>
#include <fmt/ranges.h>
#include <exception>
#include <iterator>
#include <string>
#include <vector>
#include <utility>

namespace met::detail {
  /**
   * Message class which stores a keyed list of strings, which
   * are output line-by-line in a formatted manner, in the order
   * in which they were provided.
   */
  class Message {
    std::vector<std::pair<std::string, std::string>> _messages;
    std::string _buffer;

  public:
    void put(std::string_view key, std::string_view message) {
      fmt::format_to(std::back_inserter(_buffer),
                     FMT_COMPILE("  {:<8} : {}\n"), 
                     key, 
                     message);
    }
    
    std::string get() const {
      return _buffer;
    }
  };
  
  /**
   * Exception class which stores a keyed list of strings, which
   * are output line-by-line in a formatted manner, in the order
   * in which they were provided..
   */
  class Exception : public std::exception, public Message {
    mutable std::string _what;

  public:
    const char * what() const noexcept override {
      _what = fmt::format("met::detail::Exception thrown\n{}", get());
      return _what.c_str();
    }
  };
} // namespace met::detail