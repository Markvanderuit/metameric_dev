#include <preamble.glsl>
#include <math.glsl>

// Triangle shape s.t. unculled fragments form an inquad of size [1, 1]
const vec2 elem_data[3] = vec2[3](
  vec2(0, 0),
  vec2(2, 0),
  vec2(0, 2)
);

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Buffer declarations
layout(binding = 0) restrict readonly buffer b_data { vec2 data[];  } data_in;
layout(binding = 1) restrict readonly buffer b_bary { vec4 data[];  } bary_in;
layout(binding = 2) restrict readonly buffer b_vert { vec3 data[];  } vert_in;
layout(binding = 3) restrict readonly buffer b_elem { uvec4 data[]; } elem_in;
layout(binding = 0) uniform b_unif {
  mat4  camera_matrix;
  uvec2 size_in;
  uint  n_verts;
  uint  n_quads;
} unif_in;

// Vertex output declarations
layout(location = 0) out vec2 out_value_vert;

// Internal constants
const float quad_scale  = 1.f;

void main() {
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  out_value_vert = elem_data[j];

  gl_Position = unif_in.camera_matrix * (vec4(data_in.data[i], 0, 1) 
              + vec4(quad_scale * (out_value_vert - .5f), 0, 0));
}