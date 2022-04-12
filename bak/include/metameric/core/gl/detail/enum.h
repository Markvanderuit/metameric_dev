#pragma once

#include <array>
#include <initializer_list>
#include <tuple>
#include <metameric/core/exception.h>

namespace metameric::gl {
  template <typename T, uint N>
  struct EnumMap {
    constexpr
    EnumMap(std::initializer_list<std::tuple<T, uint>> map) {
      std::copy(map.begin(), map.end(), _map.begin());
    }

    constexpr
    uint map(uint input) const {
      uint output = 0u;
      for (uint i = 0; i < N; ++i) {
        auto [ i_flag, o_flag ] = _map[i];
        if (has_flag(input, i_flag)) {
          output |= o_flag;
        }
      }
      return output;
    }

    constexpr
    uint operator[](const T &t) const {
      for (uint i = 0; i < N; ++i) {
        auto [ i_flag, o_flag ] = _map[i];
        if (i_flag == t) {
          return o_flag;
        }
      }
      runtime_assert(false, "EnumMap accessed with non-mapped value");
      return 0;
    }

    std::array<std::tuple<T, uint>, N> _map;
  };
} // namespace metameric::gl