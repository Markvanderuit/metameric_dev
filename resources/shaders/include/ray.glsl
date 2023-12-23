#ifndef RAY_GLSL_GUARD
#define RAY_GLSL_GUARD

#include <math.glsl>
#include <random_uniform.glsl>

struct AABB {
  vec3 minb; // Minimum of bounding box
  vec3 maxb; // Maximum of bounding box
};

// An object defining a 3-dimensional ray.
// Is generally the input of ray_intersect(...)
// With minor output data packed in padded space
struct Ray {
  vec3  o;
  float t;
  vec3  d;
  uint  data; // Padded data used to embed hit object/primitive ids, or hit/miss for shadow rays
};

// Flag value to indicate no object was hit
#define OBJECT_INVALID 0x000000FFu // 8 bits specifically, as we pack the index this precision

// Helper funtions to embed minor hit data in ray padding
void set_ray_data_prim(inout Ray ray, in uint i) { bitfieldInsert(ray.data, i, 0, 24); }
void set_ray_data_objc(inout Ray ray, in uint i) { bitfieldInsert(ray.data, i, 24, 8); }
uint get_ray_data_prim(in    Ray ray)            { return bitfieldExtract(ray.data, 0, 24); }
uint get_ray_data_objc(in    Ray ray)            { return bitfieldExtract(ray.data, 24, 8); }
void set_ray_data_anyh(inout Ray ray, in bool b) { ray.data = uint(b); }

#define PARENS ()
#define EXPAND(...)  EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
#define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
#define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
#define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
#define EXPAND1(...) __VA_ARGS__

#define FOR_EACH(macro, ...)                                    \
  __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define FOR_EACH_HELPER(macro, a1, ...)                         \
  macro(a1)                                                     \
  __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

struct PathInfoAOSPack0 { vec4 data; };
struct PathInfoAOSPack1 { vec4 data; };
struct PathInfoAOSPack2 { vec4 data; };
struct PathInfoSOAPack  {
  vec4 data_0;
  vec4 data_1;
  vec4 data_2;
};

#define SOA_ARG(dst, src, i) \
  dst = src[i];
#define SOA_ARG_PAIR(pair) \
  SOA_ARG pair

// Variadic macro to take a Type and N buffers, and to generate
// a function that loads the type from a SOA to a AOS format
#define ApplySOA(Ty, ...)               \
  void soa(inout Ty ty, in uint i) {    \
    FOR_EACH(SOA_ARG_PAIR, __VA_ARGS__) \
  }

#define ApplySOAInline(...) \
  FOR_EACH(SOA_ARG_PAIR, __VA_ARGS__)

// // ApplySOA(PathInfoSOAPack, 
// //         (data_0, buffer_path_pack_0, i), 
// //         (data_1, buffer_path_pack_1, i), 
// //         (data_2, buffer_path_pack_2, i))

// void load(inout PathInfo pi, in uint i) {
//   PathInfoSOAPack p;
//   // p.data_0 = buffer_path_pack_0.data[i];
//   // p.data_1 = buffer_path_pack_1.data[i];
//   // p.data_2 = buffer_path_pack_2.data[i];
//   // ApplySOAInline((p.data_0, buffer_path_pack_0.data, i), 
//   //                (p.data_1, buffer_path_pack_1.data, i), 
//   //                (p.data_2, buffer_path_pack_2.data, i))
  
//   // soa(p, i);
//   // ... Assign or unpack values 
// }

// The PathInfo object stores the general state for a path that is in-flight.
// If a path is terminated, values are reset, but pixel/state are preserved.
// Size: 64 by
struct PathInfo {
  // 12 by
  uint  pixel;       // Packed pixel position,      Retained for new paths
  uint  depth;       // Current path length,        initialized to 0 for new paths
  bool  did_scatter; // Did path scatter,           initialized to false for new paths

  // 12 by
  uint  state;      // Sampler state
  float radiance;   // Accumulated result,          initialized to 0 for new paths
  float wavelength; // Sampled wavelength,          regenerated for new paths
  
  // 12 by
  float throughput; // Current throughput of path,  initialized to 1 for new paths
  float p;          // Current probability of path, initialized to 1 for new paths
  float eta;        // Multiplied refractive index, initialized to 1 for new paths

  // 12 by
  uint  ray_extend_i;   // Index of last submitted extend ray in work queue
  float ray_extend_t;   // Throughput along extend ray
  float ray_extend_pdf; // Probability along path with extend ray

  // 12 by
  uint  ray_shadow_i;   // Index of last submitted shadow ray in work queu
  float ray_shadow_e;   // Throughput along shadow ray, multiplied by energy
  float ray_shadow_pdf; // Probability along path with shadow ray

  // 4 by, to get it to 64 bytes
  uint padd;
};

// Extract packed pixel data from PathInfo
ivec2 get_path_pixel(in PathInfo pi) {
  return ivec2(pi.pixel & 0xFFFF, pi.pixel >> 16);
}

void reset(inout PathInfo pi) {
  pi.did_scatter  = false;
  pi.radiance     = 0.f;
  pi.wavelength   = next_1d(pi.state); // TODO: Sample vec4, and sample by cdf of CMFS * avg(Le)
  pi.throughput   = 1.f;
  pi.p            = 1.f;
  pi.eta          = 1.f;
  pi.ray_extend_i = UINT_MAX;
  pi.ray_shadow_i = UINT_MAX;
}

#endif // RAY_GLSL_GUARD