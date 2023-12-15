#include <preamble.glsl>

// Triangle shape s.t. unculled fragments form an inquad of size [1, 1]
const vec2 elem_data[3] = vec2[3](
  vec2(-1.732, -1),
  vec2( 1.732, -1),
  vec2( 0,      2)
);

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_posi_buffer { vec3  data[]; } posi_buffer;
layout(binding = 1) restrict readonly buffer b_size_buffer { float data[]; } size_buffer;
layout(binding = 2) restrict readonly buffer b_colr_buffer { vec4  data[]; } colr_buffer;

// Uniform buffer declarations
layout(binding = 0) uniform b_unif_buffer {
  mat4 matrix;
  vec2 aspect;
} unif_buffer;

// Vertex output declarations
layout(location = 0) out vec2 out_posi;
layout(location = 1) out vec4 out_colr;

void main() {
  // Index of relevant embedding and triangle vert
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  // Set outputs
  out_posi = elem_data[j];
  out_colr = colr_buffer.data[i];

  // Set projected vertex position, plus billboard offset
  gl_Position = unif_buffer.matrix 
              * vec4(posi_buffer.data[i], 1)
              + vec4(size_buffer.data[i] * unif_buffer.aspect * out_posi, 0, 0);
}