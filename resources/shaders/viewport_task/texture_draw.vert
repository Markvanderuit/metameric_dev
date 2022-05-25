#version 460 core

layout (location = 0) in vec3 texture_value_in;
layout (location = 0) out vec3 texture_value_out;

layout (location = 0) uniform mat4 model_matrix;
layout (location = 1) uniform mat4 view_matrix;
layout (location = 2) uniform mat4 projection_matrix;

void main() {
  texture_value_out = texture_value_in;
  gl_Position = projection_matrix 
              * view_matrix 
              * model_matrix
              * vec4(texture_value_in, 1);
}