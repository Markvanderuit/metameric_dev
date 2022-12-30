#version 460 core

layout(location = 0) in vec2  in_value_vert;
layout(location = 0) out vec2 out_value_vert;

layout(location = 0) uniform mat4  u_model_matrix;
layout(location = 1) uniform mat4  u_camera_matrix;
layout(location = 2) uniform vec3  u_position;
layout(location = 3) uniform vec2  u_aspect;
layout(location = 4) uniform float u_size;

void main() {
  out_value_vert = in_value_vert;

  gl_Position = u_camera_matrix * u_model_matrix * vec4(u_position, 1) 
              + u_size * vec4(u_aspect * out_value_vert, 0, 0);
}