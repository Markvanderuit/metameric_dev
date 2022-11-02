#version 460 core

layout(location = 0) in  vec2 value_vert_in;
layout(location = 1) in  vec3 value_inst_in;
layout(location = 0) out vec3 value_inst_out;
layout(location = 1) out vec3 value_out;

layout(location = 0) uniform mat4  u_model_matrix;
layout(location = 1) uniform mat4  u_camera_matrix;
layout(location = 2) uniform float u_point_radius;

void main() {
  value_out      = u_point_radius * vec3(value_vert_in, 0);
  value_inst_out = value_out + 
  gl_Position = u_camera_matrix * u_point_radius * vec4(value_vert_in, 0, 1)
              + u_camera_matrix * u_model_matrix * vec4(value_inst_in, 1);
}