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

  // Helper view; replaces std::ranges::to<Ty> given it is only supported from gcc 14 or something
  template <rng::range CTy> requires (!rng::view<CTy>)
  inline constexpr auto view_to() { return detail::view_to<CTy>{}; }

  // Helper view; iterate a range and return [i, item] enumated view
  inline constexpr auto enumerate_view = [](rng::viewable_range auto &&r) {
    return vws::zip(vws::iota(0u, static_cast<unsigned>(r.size())), r);
  };

  // Helper view; pass an index and extract a reference to item inside a range
  inline constexpr auto index_into_view = [](rng::viewable_range auto &&r) {
    return vws::transform([r = rng::ref_view { r }](unsigned i) { return r[i]; });
  };
} // namespace met