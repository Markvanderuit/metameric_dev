#pragma once

#include <metameric/core/math.hpp>
#include <metameric/core/json.hpp>
#include <array>

namespace met {
  // Representation of record data used, generated, and stored by render/query primitives
  // and in surface-based uplifting constraints
  class SurfaceRecord {
    constexpr static uint record_invalid_data = 0xFFFFFFFF;
    constexpr static uint record_object_flag  = 0x00000000;
    constexpr static uint record_emitter_flag = 0x80000000;

  public:
    uint data;
    
    bool is_valid()    const { return data != record_invalid_data;       }
    bool is_emitter()  const { return (data & record_emitter_flag) != 0; }
    bool is_object()   const { return (data & record_emitter_flag) == 0; }
    uint object_i()    const { return (data >> 24) & 0x0000007F;       }
    uint emitter_i()   const { return (data >> 24) & 0x0000007F;       }
    uint primitive_i() const { return data & 0x00FFFFFF;               }
    
  public:
    SurfaceRecord() : data(record_invalid_data) { }
  
    static SurfaceRecord invalid() { return SurfaceRecord(); }

    constexpr auto operator<=>(const SurfaceRecord &) const = default;
  };
  static_assert(sizeof(SurfaceRecord) == 4);

  // Simple info object describing a surface interaction, without
  // local shading information as it is unnecessary in the struct's
  // limited use case
  struct SurfaceInfo {
    // Geometric surface data
    eig::Vector3f p;
    eig::Vector3f n;
    eig::Vector2f tx;

    // Umderlying object material data
    Colr diffuse;
    
    // Underlying record used to build SurfaceInfo object
    SurfaceRecord record;
  
  public:
    bool is_valid() const { return record.is_valid(); }

    static SurfaceInfo invalid() { 
      return { .p      = 0.f,
               .record = SurfaceRecord::invalid() }; 
    }

    bool operator==(const SurfaceInfo &o) const {
      return p.isApprox(o.p)   &&
             n.isApprox(o.n)   &&
             tx.isApprox(o.tx) &&
             record.data == o.record.data;
    }
  };

  // Ray with a surface record packed inside
  // returned by some render queries
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

  // Vertex with a surface record packed inside
  // Used inside PathRecord
  struct VertexRecord {
    // World hit position
    eig::Array3f p;

    // Record storing surface data; object/emitter/primitive id
    SurfaceRecord record;
  };
  static_assert(sizeof(VertexRecord) == 16);
  
  // A queried path object
  struct PathRecord {
    constexpr static uint path_max_depth = 4;

  public:
    // Sampled path wavelengths
    alignas(16) eig::Array4f wavelengths;

    // Energy over probability density
    // Note: if generated with PartialPathQuery(...), reflectances are ignored
    // along paths.
    alignas(16) eig::Array4f L;

    // Actual length of path before termination
    alignas(16) uint path_depth;

    // PathRecord vertex information, up to maximum depth
    alignas(16) std::array<VertexRecord, path_max_depth> data;
  };
  static_assert(sizeof(PathRecord) == (3 + PathRecord::path_max_depth) * 16);

  // JSON (de)serialization of surface info
  void from_json(const json &js, SurfaceInfo &si);
  void to_json(json &js, const SurfaceInfo &si);
} // namespace met