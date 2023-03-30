#version 460 core

#include <math.glsl>

// Triangle shape s.t. unculled fragments form an incircle with radius 1 and center (0, 0)
const vec2 triangle[3] = vec2[3](
  vec2(-1.732, -1),
  vec2( 1.732, -1),
  vec2( 0,      2)
);

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Shader storage buffer declarations
layout(binding = 0) restrict readonly buffer b_posi { vec3  data[]; } posi_buffer;
layout(binding = 1) restrict readonly buffer b_size { float data[]; } size_buffer;

// Uniform buffer declarations
layout(binding = 0) uniform b_camera {
  mat4 matrix;
  vec2 aspect;
} camera;
layout(binding = 1) uniform b_value { // unused in stage
  vec4 value;
} unif;

// Uniform declarations
layout(location = 0) uniform vec4 u_value;

// Vertex stage declarations
layout(location = 0) out vec2 out_value_vert;

void main() {
  uint i = gl_VertexID / 3, j = gl_VertexID % 3;

  // Load required data
  vec2  vert = triangle[j];
  vec3  offs = posi_buffer.data[i];
  float size = size_buffer.data[i];

  out_value_vert = vert;
  gl_Position = camera.matrix * vec4(offs, 1) 
              + size * vec4(camera.aspect * vert, 0, 0);
}