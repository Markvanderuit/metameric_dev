#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/surface.hpp>
#include <array>

namespace met {
  constexpr uint path_max_depth = 8;

  // Ray with a surface record packed inside
  struct RayRecord {
    eig::Vector3f o;
    float         t;
    eig::Vector3f d;
    SurfaceRecord record;

  public:
    eig::Vector3f get_position() const {
      return t == std::numeric_limits<float>::max() 
        ? eig::Vector3f(std::numeric_limits<float>::max())
        : o + t * d;
    }

    static RayRecord invalid() {
      return RayRecord { .o      = 0,
                         .t      = std::numeric_limits<float>::max(),
                         .d      = 0,
                         .record = SurfaceRecord::invalid() };
    }
  };
  static_assert(sizeof(RayRecord) == 32);

  // A single vertex in a queried path object, with a surface record packed inside
  struct PathVertexRecord {
    // World hit position
    eig::Array3f p;

    // Record storing surface data; object/emitter/primitive id
    SurfaceRecord record;
  };
  static_assert(sizeof(PathVertexRecord) == 16);
  
  // A queried path object
  struct PathRecord {
    // Sampled path wavelengths
    alignas(16) eig::Array4f wavelengths;

    // Energy over probability density
    // Note: if generated with PartialPathQuery(...), reflectances are ignored
    // along paths.
    alignas(16) eig::Array4f L;

    // Actual length of path before termination
    alignas(16) uint path_depth;

    // PathRecord vertex information, up to maximum depth
    alignas(16) std::array<PathVertexRecord, path_max_depth> data;
  };
  static_assert(sizeof(PathRecord) == (3 + path_max_depth) * 16);
} // namespace met