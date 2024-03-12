#pragma once

#include <ranges>

namespace met {
  // Namespace shorthands
  namespace rng = std::ranges;
  namespace vws = std::views;

  // Helper view; iterate a range and return [i, item] enumated view
  inline constexpr auto enumerate_view = [](rng::viewable_range auto &&r) {
    return vws::zip(vws::iota(0u, static_cast<unsigned>(r.size())), r);
  };

  // Helper view; pass an index and extract a reference to item inside a range
  inline constexpr auto index_into_view = [](rng::viewable_range auto &&r) {
    return vws::transform([r = rng::ref_view { r }](unsigned i) { return r[i]; });
  };
} // namespace met