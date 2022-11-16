#version 460 core

layout(std430)      buffer;
layout(binding = 0) restrict readonly buffer b_0 { uint data[]; } b_vali_texture;

layout(location = 0) in  vec2 in_value_vert;
layout(location = 1) in  vec3 in_value_posi;
layout(location = 2) flat in uint in_value_indx;
layout(location = 0) out vec3 out_value_colr;

void main() {
  if (length(in_value_vert) > 1.f) {
    discard;
  }

  out_value_colr = (b_vali_texture.data[in_value_indx] == 0 ? in_value_posi : vec3(1, 0, 0));
}