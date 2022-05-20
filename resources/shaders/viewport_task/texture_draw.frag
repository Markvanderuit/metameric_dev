#version 460 core

layout (location = 0) in vec3 texture_value_in;
layout (location = 0) out vec3 color_value_out;

layout (location = 2) uniform ivec2 viewport_size;

void main() {
  vec2 viewport_pos = abs(gl_FragCoord.xy) / vec2(viewport_size); // 4 ms
  color_value_out = vec3(viewport_pos, 0);
  // color_value_out = texture_value_in;
}
