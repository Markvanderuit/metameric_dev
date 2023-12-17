#include <preamble.glsl>
#include <gbuffer.glsl>
#include <scene.glsl>

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input/output declarations
layout(location = 0) in  vec3      in_value_p;
layout(location = 1) in  flat uint in_value_id;
layout(location = 0) out uvec4     out_value;

void main() {
  GBufferRay gb = { in_value_p, 0 };
  out_value = pack_gbuffer_ray(gb);
}