#include <preamble.glsl>
#include <math.glsl>
#include <render/gbuffer.glsl>
#include <render/scene.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input declarations
layout(location = 0) in vec3      in_value_n;
layout(location = 1) in vec2      in_value_tx;
layout(location = 2) in flat uint in_value_id;

// Fragment output declarations
layout(location = 0) out vec4 out_value; // G-Buffer encoding

// Buffer declarations
layout(binding = 0) uniform b_buff_unif {
  mat4  trf;
} buff_unif;
layout(binding = 1) uniform b_buff_objects {
  uint n;
  ObjectInfo data[32];
} buff_objects;
layout(binding = 2) uniform b_buff_meshes {
  uint n;
  MeshInfo data[32];
} buff_meshes;

void main() {
  out_value = uintBitsToFloat(encode_gbuffer(gl_FragCoord.z, in_value_n, in_value_tx, in_value_id));
}