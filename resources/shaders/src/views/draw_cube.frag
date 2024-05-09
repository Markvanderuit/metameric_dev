#include <preamble.glsl>
#include <render/load/defaults.glsl>
#include <render/sensor.glsl>

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;
layout(early_fragment_tests) in;

// Uniform buffer declaration
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf;
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;

// Fragment stage declarations
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec4 value_out;

void main() {
  value_out = vec4(value_in, 1);
}
