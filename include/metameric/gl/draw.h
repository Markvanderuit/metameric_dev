#pragma once

#include <metameric/gl/detail/fwd.h>
#include <metameric/gl/detail/handle.h>

namespace metameric::gl {
  struct DrawInfo {
    // Draw information
    PrimitiveType type;
    const Vertexarray *array;

    // Vertex range data
    uint vertex_count;
    uint vertex_first = 0;

    // Instancing data
    uint instance_count = 0;
    uint vertex_base = 0;
    uint instance_base = 0;

    // Optional bindable program
    const Program *program = nullptr;
  };

  struct DrawIndirectInfo {
    // Draw information
    PrimitiveType type;
    const Vertexarray *array;

    // Indirect buffer
    const Buffer *buffer;

    // Optional bindable program
    const Program *program = nullptr; 
  };

  struct ComputeInfo {
    // Dispatch dimensions
    uint groups_x = 1;
    uint groups_y = 1;
    uint groups_z = 1;
    
    // Optional bindable program
    const Program *program = nullptr;
  };

  struct ComputeIndirectInfo {
    // Indirect buffer
    const Buffer *buffer;

    // Optional bindable program
    const Program *program = nullptr;
  };

  // Dispatch a draw operation
  void draw(DrawInfo info);
  void draw(DrawIndirectInfo info);

  // Dispatch a compute operation
  void compute(ComputeInfo info);
  void compute(ComputeIndirectInfo info); 
} // namespace metameric::gl