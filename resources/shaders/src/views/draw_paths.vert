#include <preamble.glsl>
#include <math.glsl>
#include <spectrum.glsl>
#include <distribution.glsl>
#include <render/detail/path_types.glsl>
#include <render/load/cmfs.glsl>
#include <render/load/defaults.glsl>

// Buffer layout declarations
layout(std140) uniform;
layout(std430) buffer;

// Uniform buffer declarations
layout(binding = 0) uniform b_buff_sensor {
  mat4  full_trf;
  mat4  proj_trf;
  mat4  view_trf;
  uvec2 film_size; 
} buff_sensor;

// Storage buffer declarations
layout(binding = 0) restrict readonly buffer b_buff_paths { 
  uint head;
  uint padd[3]; /* align head to 16 bytes */ 
  Path data[];
} buff_paths;

// Stage output declarations
layout(location = 0) out vec3 out_value_c;

// Sampler components
layout(binding = 0) uniform sampler1DArray b_cmfs_3f; // Observer function data, 3 components

// Declare access to cmfs and wavelength data for sensor code
declare_scene_cmfs_data(b_cmfs_3f);

// All includes from and in here rely on buffers/samplers to be declared
#include <render/ray.glsl>
#include <render/sensor.glsl>

void main() {
  uint path_i = (gl_VertexID / 2) / path_max_depth;
  uint depth  = buff_paths.data[path_i].path_depth;
  uint vert_i = min(((gl_VertexID % (path_max_depth * 2)) + 1) / 2, max(1, depth) - 1);

  // Generate output color
  out_value_c = sensor_apply(buff_paths.data[path_i].wvls, 
                             buff_paths.data[path_i].L);

  // Generate output vertex position
  gl_Position = buff_sensor.full_trf * vec4(buff_paths.data[path_i].data[vert_i].p, 1);
}