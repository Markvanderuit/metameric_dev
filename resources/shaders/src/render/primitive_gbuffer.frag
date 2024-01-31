#include <preamble.glsl>
#include <render/gbuffer.glsl>
#include <render/ray.glsl>
#include <render/load/defaults.glsl>
#include <render/scene.glsl>

// Fragment early-Z declaration
layout(early_fragment_tests) in;

// Fragment input/output declarations
layout(location = 0) in  vec3      in_value_p;
layout(location = 1) in  flat uint in_value_i;
layout(location = 0) out uvec4     out_value;

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf; 
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;
layout(binding = 1) uniform b_buff_objects {
  uint n;
  ObjectInfo data[max_supported_objects];
} buff_objects;
layout(binding = 2) uniform b_buff_meshes {
  uint n;
  MeshInfo data[max_supported_meshes];
} buff_meshes;

Ray ray_from_sensor() {
  // Get necessary sensor information
  float tan_y    = 1.f / buff_sensor.proj_trf[1][1];
  float aspect   = float(buff_sensor.film_size.x) / float(buff_sensor.film_size.y);
  mat4  view_inv = inverse(buff_sensor.view_trf);

  // Get pixel center in [-1, 1]
  vec2 xy = (vec2(gl_FragCoord.xy) / vec2(buff_sensor.film_size) - .5f) * 2.f;
  
  // Generate camera ray
  Ray ray;
  ray.o = (view_inv * vec4(0, 0, 0, 1)).xyz;
  ray.d = normalize((view_inv * vec4(xy.x * tan_y * aspect, xy.y * tan_y, -1, 0)).xyz);
  ray.t = FLT_MAX;

  return ray;
}

void main() {
  GBufferRay gb = { 
    in_value_p, 
    in_value_i, /* << 24 | */
    /* mesh_info.prims_offs +  */gl_PrimitiveID /* ( & 0x00FFFFFF)  */// 8 bits for object id, 24 bits for primitive id
  };
  
  out_value = pack_gbuffer_ray(gb);
}