#version 460 core

layout (location = 0) in vec3 texture_value_in;
layout (location = 0) out vec3 texture_value_out;

layout (location = 0) uniform mat4 model_matrix;
layout (location = 1) uniform mat4 camera_matrix;

void main() {
  texture_value_out = texture_value_in;
  gl_Position = camera_matrix 
              * model_matrix
              * vec4(texture_value_in, 1);
}