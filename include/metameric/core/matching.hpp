#pragma once

#include <concepts>
#include <optional>
#include <tuple>
#include <variant>

namespace met {
  namespace detail {
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
  } // namespace detail
  
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
} // namespace met