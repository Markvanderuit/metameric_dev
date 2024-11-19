#include <preamble.glsl>
#include <render/load/defaults.glsl>

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;
layout(early_fragment_tests) in;

// Uniform buffer declaration
layout(binding = 1) uniform b_buff_settings {
  float alpha;
} buff_settings;

// Fragment stage declarations
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec4 value_out;

void main() {
  value_out = vec4(value_in, buff_settings.alpha);
}
