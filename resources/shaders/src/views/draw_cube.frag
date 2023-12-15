#include <preamble.glsl>

// Enable early-z for this fragment shader
layout(early_fragment_tests) in;

// Input/output variables
layout(location = 0) in vec4  value_in;
layout(location = 0) out vec4 value_out;

void main() {
  value_out = value_in;
}
