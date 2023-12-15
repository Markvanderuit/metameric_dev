#include <preamble.glsl>
#include <math.glsl>

// Tree node data structure
struct Node {
  vec3 minb;
  uint i;
  vec3 maxb;
  uint n;
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

const uint minb_data [8]= uint[8](
  7, // 111 - 000 ; 0    ... 0
  3, // 011 - 100 ; 4    ... 1
  1, // 001 - 110 ; 6    ... 2
  5, // 101 - 010 ; 2    ... 3
  
  6, // 110 - 001 ; 1    ... 4
  2, // 010 - 101 ; 5    ... 5
  0, // 000 - 111 ; 7    ... 6
  4  // 100 - 011 ; 3    ... 7
);

const uint elem_data[48] = uint[48](
  0, 1, 1, 2, 2, 3, 3, 0,
  1, 5, 5, 6, 6, 2, 2, 1,
  0, 3, 3, 7, 7, 4, 4, 0,
  0, 4, 4, 5, 5, 1, 1, 0,
  3, 2, 2, 6, 6, 7, 7, 3,
  5, 4, 4, 7, 7, 6, 6, 5
);

// Buffer declarations
layout(binding = 0) restrict readonly buffer b_tree { Node data[]; } tree_in;
layout(binding = 0) uniform b_unif {
  uint nodi;
  uint nodn;
} unif;
layout(binding = 1) uniform b_camr {
  mat4 matrix;
  vec2 aspect;
} camera;

void main() {
  uint i = unif.nodi + gl_VertexID / 48, j = gl_VertexID % 48;

  uint mult_minb_i = minb_data[elem_data[j]];
  uint mult_maxb_i = 7 - mult_minb_i;
  
  Node node = tree_in.data[i];
  if (node.n == 0) {
    gl_Position = vec4(0);
  } else if (node.n == 1) {
    vec3 v = (node.minb - vec3(0.0025)) * mult_data[mult_minb_i] +
             (node.maxb + vec3(0.0025)) * mult_data[mult_maxb_i];
    gl_Position = camera.matrix * vec4(v, 1);
  } else {
    vec3 v = node.minb * mult_data[mult_minb_i] +
             node.maxb * mult_data[mult_maxb_i];
    gl_Position = camera.matrix * vec4(v, 1);
  }
}