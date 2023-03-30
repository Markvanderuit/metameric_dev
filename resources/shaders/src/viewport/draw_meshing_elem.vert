#version 460 core

// Vertex stage declarations
layout(location = 0) in vec3  value_in;
layout(location = 0) out vec3 value_out;

// Uniform buffer declarations
layout(binding = 0, std140) uniform b_camera {
  mat4 matrix;
  vec2 aspect;
} camera;

void main() {
  value_out = value_in;
  gl_Position = camera.matrix * vec4(value_in.xyz, 1);
}