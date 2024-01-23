#pragma once

#include <metameric/core/math.hpp>
#include <array>

namespace met {
  constexpr uint path_max_depth    = 4;
  constexpr uint path_invalid_data = 0xFFFFFFFF;
  constexpr uint path_emitter_flag = 0x80000000;
  constexpr uint path_object_flag  = 0x00000000;

  // A single vertex in a queried path object
  struct PathVertex {
    // World hit position
    eig::Array3f p;

    // Record storing surface data; object/emitter/primitive id
    uint data;

  public:
    // Data extraction methods
    bool surface_is_valid()    const { return data != path_invalid_data;       }
    bool surface_is_emitter()  const { return (data & path_emitter_flag) != 0; }
    bool surface_is_object()   const { return (data & path_emitter_flag) == 0; }
    uint surface_object_i()    const { return (data >> 24) & 0x0000007F;       }
    uint surface_emitter_i()   const { return (data >> 24) & 0x0000007F;       }
    uint surface_primitive_i() const { return data & 0x00FFFFFF;               }
  };
  static_assert(sizeof(PathVertex) == 16);
  
  // A queried path object
  struct PathInfo {
    // Sampled path wavelengths
    alignas(16) eig::Array4f wavelengths;

    // Energy times geometric attenuation over probability density,
    // without reflectances which are separated out
    alignas(16) eig::Array4f L;

    // Total length of path before termination
    alignas(16) uint path_depth;

    // Path vertex information, up to path_depth;
    alignas(16) std::array<PathVertex, path_max_depth> data;
  };
  static_assert(sizeof(PathInfo) == 112);
} // namespace met