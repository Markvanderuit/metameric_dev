#pragma once

#include <ranges>

namespace met {
  // Namespace shorthands
  namespace rng = std::ranges;
  namespace vws = std::views;

  // Helper code for met::view_to<Ty>
  // Src: https://stackoverflow.com/questions/58808030/range-view-to-stdvector
  namespace detail {
    // Type acts as a tag to find the correct operator| overload
    template <typename C>
    struct view_to { };
    
    // This actually does the work
    template <typename CTy, rng::range RTy> requires (std::convertible_to<rng::range_value_t<RTy>, typename CTy::value_type>)
    CTy operator|(RTy&& r, view_to<CTy>) {
      return CTy { r.begin(), r.end() };
    }
  } // namespace detail

  // Helper code for met::view_zip
  // Src: https://github.com/alemuntoni/zip-views/blob/master/zip_view.hpp
  namespace detail {
    template <typename ... Args, std::size_t ... Index>
    auto any_match_impl(std::tuple<Args...> const & lhs,
      std::tuple<Args...> const & rhs,
      std::index_sequence<Index...>) -> bool
    {
      auto result = false;
      result = (... | (std::get<Index>(lhs) == std::get<Index>(rhs)));
      return result;
    }

    template <typename ... Args>
    auto any_match(std::tuple<Args...> const & lhs, std::tuple<Args...> const & rhs)
      -> bool
    {
      return any_match_impl(lhs, rhs, std::index_sequence_for<Args...>{});
    }

    template <std::ranges::viewable_range ... Rng>
    class zip_iterator {
      std::tuple<std::ranges::iterator_t<Rng>...> m_iters;
    
    public:
      using value_type = std::tuple<std::ranges::range_reference_t<Rng>...>;

      zip_iterator() = delete;

      zip_iterator(std::ranges::iterator_t<Rng> && ... iters)
      : m_iters {std::forward<std::ranges::iterator_t<Rng>>(iters)...} { }

      auto operator++() -> zip_iterator&  {
        std::apply([](auto && ... args){ ((++args), ...); }, m_iters);
        return *this;
      }

      auto operator++(int) -> zip_iterator  {
        auto tmp = *this;
        ++*this;
        return tmp;
      }

      auto operator!=(zip_iterator const & other) const {
        return !(*this == other);
      }

      auto operator==(zip_iterator const & other) const {
        auto result = false;
        return any_match(m_iters, other.m_iters);
      }

      auto operator*() -> value_type {
        return std::apply([](auto && ... args){ return value_type(*args...); }, m_iters);
      }
    };

    template <std::ranges::viewable_range ... T>
    class zipper {
      std::tuple<T ...> m_args;
    public:
      using zip_type = zip_iterator<T ...>;

      template <typename ... Args>
      zipper(Args && ... args)
      : m_args{std::forward<Args>(args)...} { }

      auto begin() -> zip_type {
        return std::apply([](auto && ... args){ return zip_type(std::ranges::begin(args)...); }, m_args);
      }

      auto end() -> zip_type {
        return std::apply([](auto && ... args){ return zip_type(std::ranges::end(args)...); }, m_args);
      }
    };
  } // namespace detail

  // Helper view; replaces std::ranges::to<Ty> given it is only supported from gcc-14 or something
  template <rng::range CTy> requires (!rng::view<CTy>)
  inline constexpr auto view_to() { return detail::view_to<CTy>{}; }

  // Helper view; replaces std::views::zip given it's only supported from gcc-13 or something
  template <rng::viewable_range ... T>
  inline constexpr auto view_zip(T && ... t) { return detail::zipper<T ...>{std::forward<T>(t)...}; }

  // Helper view; iterate a range and return [i, item] enumated view
  inline constexpr auto enumerate_view = [](rng::viewable_range auto &&r) {
    return view_zip(vws::iota(0u, static_cast<unsigned>(r.size())), r);
  };

  // Helper view; pass an index and extract a reference to item inside a range
  inline constexpr auto index_into_view = [](rng::viewable_range auto &&r) {
    return vws::transform([r = rng::ref_view { r }](unsigned i) { return r[i]; });
  };
} // namespace met