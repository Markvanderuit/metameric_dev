#include <concepts>
#include <optional>
#include <variant>

namespace met {
  namespace detail {
    // Helper types for tuple_visit and variant_visit
    // Src: https://stackoverflow.com/questions/57642102/how-to-iterate-over-the-types-of-stdvariant
    
    template <std::size_t I> 
    using index_t = std::integral_constant<std::size_t, I>;

    template <std::size_t I> 
    constexpr index_t<I> index{};

    template <std::size_t...Is> 
    constexpr std::tuple< index_t<Is>... > make_indexes(std::index_sequence<Is...>) { 
      return std::make_tuple(index<Is>...); 
    }
    
    template<std::size_t N>
    constexpr auto indexing_tuple = make_indexes(std::make_index_sequence<N>{ });

    template <std::size_t...Is, class T, class F>
    auto tuple_visit(std::index_sequence<Is...>, T&& tup, F&& f ) {
      (f(std::get<Is>(std::forward<T>(tup))), ...);
    }

    template <class T, class F>
    auto tuple_visit(T&& tup, F&& f) {
      auto indexes = std::make_index_sequence<std::tuple_size_v<std::decay_t<T>>>{ };
      return tuple_visit(indexes, std::forward<T>(tup), std::forward<F>(f));
    }
  } // namespace detail

  // Visit a variant, s.t. the arguments passed to the capture
  // form default-initialized objects of the variant's underlying type,
  // and a boolean indicating whether the type is matched. Useful
  // for type selectors in UI components
  /* 
    variant | match_type([](auto default_of_type, bool is_match) {
      ...
    });
   */
  template <typename T> struct match_type : T { using T::operator(); };
  template <typename T> match_type(T) -> match_type<T>;
  template <typename... Ts, typename F>
  constexpr decltype(auto) operator| (std::variant<Ts...> const& v, match_type<F> const& f) {
    using VTy = std::variant<Ts...>;
    auto indexes = detail::indexing_tuple<std::variant_size_v<VTy>>;
    return tuple_visit(indexes, [&v, f](auto I) {
      using ATy = std::variant_alternative_t<I, VTy>;
      return f(ATy(), std::holds_alternative<ATy>(v));
    });
  }
  
  // Syntactic sugar for 'std::visit(visitor, variant) <- variant | match { visitors... };'
  // Src: https://en.cppreference.com/w/cpp/utility/variant/visit
  // Src: https://github.com/AVasK/vx
  /*
    variant | match {
      [](uint i)  { ... },
      [](float f) { ... },
      [](auto v)  { ... },
    }
  */
  template <typename... Ts> struct match : Ts... { using Ts::operator()...; };
  template <typename... Ts> match(Ts...) -> match<Ts...>;
  template <typename... Ts, typename... Fs>
  constexpr decltype(auto) operator| (std::variant<Ts...> const& v, match<Fs...> const& f) {
    return std::visit(f, v);
  }

  // Syntactic sugar for a variant | match { visitors } pattern over
  // all the known types of an std::optional, matching only if the
  // underlying type is present
  template <typename T, typename... Fs>
  requires (std::is_invocable_v<match<Fs...>, T> && std::is_invocable_v<match<Fs...>>)
  constexpr decltype(auto) operator| (std::optional<T> const& o, match<Fs...> const& match) {
    if (o.has_value()) 
      return match(o.value());
    else 
      return match();
  }

  // Syntactic sugar for 'std::visit(visitor, variant) <- variant | match_one(visitor);'
  /*
    variant | match_one([](uint i)  { ... });
  */
  template <typename T> struct match_one : T { using T::operator(); };
  template <typename T> match_one(T) -> match_one<T>;
  template <typename... Ts, typename F>
  constexpr void operator| (std::variant<Ts...> const& v, match_one<F> const& f) {
    v | match { f, [](const auto &) {} };
  }
} // namespace met