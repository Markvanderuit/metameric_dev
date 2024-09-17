#include <preamble.glsl>
#include <render/record.glsl>
#include <render/gbuffer.glsl>
#include <render/ray.glsl>
#include <render/load/defaults.glsl>
#include <render/scene.glsl>

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment stage declarations
layout(location = 0) in vec3      in_value_n;
layout(location = 1) in vec2      in_value_tx;
layout(location = 2) in flat uint in_value_rc;
layout(location = 0) out vec4     out_value_gb;

// Buffer declarations
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf; 
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;
layout(binding = 1) uniform b_buff_objects {
  uint n;
  ObjectInfo data[met_max_objects];
} buff_objects;

void main() {
  // Store primitive ID in object record
  uint rc = in_value_rc;
  record_set_object_primitive(rc, gl_PrimitiveID);

  // Output packed gbuffer data
  // out_value_gb = vec4(in_value_n, 1);
  out_value_gb = uintBitsToFloat(pack_gbuffer(
    gl_FragCoord.z, // user can recover position from depth
    in_value_n,
    in_value_tx,
    rc
  ));
}