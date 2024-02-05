#pragma once

#include <metameric/core/math.hpp>

namespace met {
  // Representation of record data used, generated, and stored by render/query primitives
  // and in surface-based uplifting constraints
  struct SurfaceRecord {
    constexpr static uint record_invalid_data = 0xFFFFFFFF;
    constexpr static uint record_emitter_flag = 0x80000000;
    constexpr static uint record_object_flag  = 0x00000000;

  public:  
    uint data;
    
  public:
    bool is_valid()    const { return data != record_invalid_data;       }
    bool is_emitter()  const { return (data & record_emitter_flag) != 0; }
    bool is_object()   const { return (data & record_emitter_flag) == 0; }
    uint object_i()    const { return (data >> 24) & 0x0000007F;       }
    uint emitter_i()   const { return (data >> 24) & 0x0000007F;       }
    uint primitive_i() const { return data & 0x00FFFFFF;               }

  public:
    SurfaceRecord() : data(record_invalid_data) {}
  
    static SurfaceRecord invalid() {
      return SurfaceRecord();
    }

    friend
    auto operator<=>(const SurfaceRecord &, const SurfaceRecord &) = default;
  };
} // namespace met