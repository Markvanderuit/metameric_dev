#version 460 core

#include <math.glsl>

// Fragment input declarations
layout(location = 0) in vec2 in_value_vert;

// Fragment output declarations
layout(location = 0) out vec4 out_value_colr;

// Buffer declarations
layout(binding = 0) restrict readonly buffer b_data { vec2 data[];  } data_in;
layout(binding = 1) restrict readonly buffer b_bary { vec4 data[];  } bary_in;
layout(binding = 2) restrict readonly buffer b_vert { vec3 data[];  } vert_in;
layout(binding = 3) restrict readonly buffer b_elem { uvec4 data[]; } elem_in;
layout(binding = 0) uniform b_unif {
  uvec2 size_in;
  vec2  viewport_aspect;
  uint  n_verts;
  uint  n_quads;
} unif_in;

void main() {
  if (clamp(in_value_vert, 0, 1) != in_value_vert)
    // out_value_colr = vec4(1, 0, 1, 1);
    discard;
  else {
    // Nearest-neighbor find corresponding 2d/1d coordinates in input size
    uvec2 ij_in = uvec2(in_value_vert * vec2(unif_in.size_in));
    uint i_in = unif_in.size_in.x * ij_in.y + ij_in.x;

    // Load relevant barycentric weights, vertex indices, constraint indices
    uint elems_offs = gl_PrimitiveID * unif_in.n_verts;
    uvec4 elems_idx = elem_in.data[floatBitsToUint(bary_in.data[i_in].w)];
    vec3 xyz  = bary_in.data[i_in].xyz;
    vec4 bary = vec4(xyz, 1.f - hsum(xyz));

    // Compute convex combination of vertex colors
    vec3 v = vec3(0);
    for (uint j = 0; j < 4; ++j)
      v += bary[j] * vert_in.data[elems_offs + elems_idx[j]];

    out_value_colr = vec4(v, 1); // vec4(in_value_vert, 0, 1);
  }
}