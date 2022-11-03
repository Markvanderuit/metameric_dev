#version 460 core

layout(location = 0) in vec2   in_value_vert; // Instanced, division 0
layout(location = 1) in vec3   in_value_posi; // Instanced, division 1
layout(location = 2) in float  in_value_size; // Instanced, division 1
layout(location = 0) out vec2  out_value_vert;

layout(location = 0) uniform mat4 u_camera_matrix;
layout(location = 1) uniform vec2 u_billboard_aspect;

void main() {
  out_value_vert = in_value_vert;

  gl_Position = u_camera_matrix * vec4(in_value_posi, 1) 
              + in_value_size * vec4(u_billboard_aspect * out_value_vert, 0, 0);
}