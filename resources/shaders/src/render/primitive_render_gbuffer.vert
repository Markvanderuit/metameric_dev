#include <preamble.glsl>
#include <render/record.glsl>
#include <render/detail/scene_types.glsl>
#include <render/load/defaults.glsl>

// General layout rule declarations
layout(std140) uniform;
layout(std430) buffer;

// Vertex stage declarations
layout(location = 0) in  uvec4     in_vert_pack;
layout(location = 0) out vec3      out_value_n;
layout(location = 1) out vec2      out_value_tx;
layout(location = 2) out flat uint out_value_rc;

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

#include <render/scene.glsl>

void main() {
  /* // Extract packed vertex data to obtain local position
  Vertex vt = unpack(to_mesh_vert_pack(in_vert_pack));
  
  // Store object ID in object record
  uint rc = RECORD_INVALID_DATA;
  record_set_object(rc, gl_DrawID); // The index in glMultiDraw* matches the index of a specific object

  // Set vertex outputs
  out_value_n  = vt.n;
  out_value_tx = vt.tx;
  out_value_rc = rc;

  // Apply camera transformation for vertex position output
  gl_Position = buff_sensor.full_trf 
              * buff_objects.data[gl_DrawID].trf_mesh
              * vec4(vt.p, 1); */
}