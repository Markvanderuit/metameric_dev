#pragma once

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/compile.h>
#include <fmt/ranges.h>
#include <concepts>
#include <exception>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

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

  // Helper types for tuple_visit and variant_visit
  // Src: https://stackoverflow.com/questions/57642102/how-to-iterate-over-the-types-of-stdvariant
  
  template <std::size_t I> 
  using index_t = std::integral_constant<std::size_t, I>;

  template <std::size_t I> 
  constexpr inline index_t<I> index {};

  template <std::size_t...Is> 
  constexpr inline std::tuple< index_t<Is>... > make_indexes(std::index_sequence<Is...>) { 
    return std::make_tuple(index<Is>...); 
  }
  
  template<std::size_t N>
  constexpr inline auto indexing_tuple = make_indexes(std::make_index_sequence<N>{ });

  template <std::size_t...Is, class T, class F>
  inline auto tuple_visit(std::index_sequence<Is...>, T&& tup, F&& f ) {
    (f(std::get<Is>(std::forward<T>(tup))), ...);
  }

  template <class T, class F>
  inline auto tuple_visit(T&& tup, F&& f) {
    auto indexes = std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{ };
    return tuple_visit(indexes, std::forward<T>(tup), std::forward<F>(f));
  }
} // namespace met::detail
