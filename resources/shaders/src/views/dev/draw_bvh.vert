#version 460 core

#include <math.glsl>

// Tree node data structure
struct Node {
  vec3 b_min;
  uint e_begin;
  vec3 b_max;
  uint e_extent;
};

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;

const vec3 mult_data[8] = vec3[8](
  vec3(0, 0, 0), // 0
  vec3(0, 0, 1), // 1
  vec3(0, 1, 0), // 2
  vec3(0, 1, 1), // 3
  vec3(1, 0, 0), // 4
  vec3(1, 0, 1), // 5
  vec3(1, 1, 0), // 6
  vec3(1, 1, 1)  // 7
);

const uint bmin_data [8]= uint[8](
  7, // 111 - 000 ; 0    ... 0
  3, // 011 - 100 ; 4    ... 1
  1, // 001 - 110 ; 6    ... 2
  5, // 101 - 010 ; 2    ... 3
  
  6, // 110 - 001 ; 1    ... 4
  2, // 010 - 101 ; 5    ... 5
  0, // 000 - 111 ; 7    ... 6
  4  // 100 - 011 ; 3    ... 7
);

const uint elem_data[36] = uint[36](
// face 0,   face 1
   2, 0, 1,  2, 3, 0,
   4, 5, 6,  4, 6, 7,
   1, 2, 6,  1, 6, 5,
   2, 3, 7,  2, 7, 6,
   3, 0, 4,  3, 4, 7,
   0, 1, 5,  0, 5, 4
);

// Buffer declarations
layout(binding = 0) restrict readonly buffer b_tree { Node data[]; } tree_in;
layout(binding = 0) uniform b_unif {
  uint node_begin;
  uint node_extent;
} unif;
layout(binding = 1) uniform b_camr {
  mat4 matrix;
  vec2 aspect;
} camera;

// Vertex stage declarations
// layout(location = 0) out vec3 out_value_vert;
// layout(location = 0) out bool out_value_discard;

void main() {
  uint i = unif.node_begin + gl_VertexID / 36, j = gl_VertexID % 36;

  uint mult_bmin_i = bmin_data[elem_data[j]];
  uint mult_bmax_i = 7 - mult_bmin_i;
  
  Node node = tree_in.data[i];
  if (node.e_extent == 0) {
    gl_Position = vec4(0);
    return;
  }

  vec3 v = node.b_min * mult_data[mult_bmin_i] +
           node.b_max * mult_data[mult_bmax_i];

  // Set per vertex position property
  // out_value_vert = v;
  gl_Position = camera.matrix * vec4(v, 1);
}