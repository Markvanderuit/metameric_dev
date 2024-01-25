#include <preamble.glsl>
#include <render/scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex input declarations
layout(location = 0) in vec4 in_vert_a;
layout(location = 1) in vec4 in_vert_b;

// Vertex output declarations
layout(location = 0) out vec3 out_value_p;
layout(location = 1) out vec3 out_value_n;
layout(location = 2) out vec2 out_value_tx;
layout(location = 3) out flat uint out_value_id;

// Buffer declarations
layout(binding = 0) uniform b_unif_camera {
  mat4 trf;
} unif_camera;
layout(binding = 0) restrict readonly buffer b_buff_objects {
  ObjectInfo[] data;
} buff_objects;

void main() {
  out_value_p  = in_vert_a.xyz;
  out_value_n  = in_vert_b.xyz;
  out_value_tx = vec2(in_vert_a.w, in_vert_b.w);
  out_value_id = gl_DrawID;

  gl_Position = unif_camera.trf 
              * buff_objects.data[gl_DrawID].trf
              * vec4(out_value_p, 1);
}