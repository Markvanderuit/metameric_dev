#include <preamble.glsl>

// Stage input/output declarations
layout(location = 0) in  vec3 in_value_c;
layout(location = 0) out vec4 out_value_c;

void main() {
  out_value_c = vec4(in_value_c, 1.f);
  // out_value_c = vec4(in_value_c, 0.01f);
}