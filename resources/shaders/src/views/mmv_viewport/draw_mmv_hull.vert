#include <preamble.glsl>
#include <render/load/defaults.glsl>

// Layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Uniform buffer declaration
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf;
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;

// Vertex stage declarations
layout(location = 0) in vec3  value_in;
layout(location = 1) in vec3  color_in;
layout(location = 0) out vec3 color_out;

void main() {
  // Value is color position, so pass through
  color_out = color_in;
  
  // Vertex position is transformed color position
  gl_Position = buff_sensor.full_trf * vec4(value_in.xyz, 1);
}