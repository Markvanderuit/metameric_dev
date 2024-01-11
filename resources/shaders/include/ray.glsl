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

  // Padded data embeds hit information
  // - for generic rays; | 1 bit, object/emitter flag | 7 bits, object/emitter id | 24 bits, object primitive id |
  //                                                    so 127 objects/emitters     so 16M primitives per obj
  // - for shadow rays:  1 bit flags hit/miss
  uint data;
};

// Flag value to indicate no object was hit
#define RAY_INVALID_DATA 0xFFFFFFFF
#define RAY_OBJECT_FLAG  0x00000000
#define RAY_EMITTER_FLAG 0x10000000

#define OBJECT_INVALID   0x000000FFu // 8 bits specifically, as we pack the index with this precision

// Helper funtions to embed minor hit data in ray padding
void ray_set_data_objc(inout Ray ray, in uint i) { ray.data = (ray.data & 0x00FFFFFF) | (i << 24);        }
void ray_set_data_prim(inout Ray ray, in uint i) { ray.data = (ray.data & 0xFF000000) | (i & 0x00FFFFFF); }
uint ray_get_data_objc(in    Ray ray)            { return (ray.data >> 24) & 0x000000FF; }
uint ray_get_data_prim(in    Ray ray)            { return (ray.data & 0x00FFFFFF);       }
void ray_set_data_anyh(inout Ray ray, in bool b) { ray.data = uint(b); }

Ray init_ray(vec3 d) {
  Ray ray;
  ray.o    = vec3(0);
  ray.d    = d;
  ray.t    = FLT_MAX;
  ray.data = RAY_INVALID_DATA;
  return ray;
}

Ray init_ray(vec3 d, float t_max) {
  Ray ray;
  ray.o    = vec3(0);
  ray.d    = d;
  ray.t    = t_max;
  ray.data = RAY_INVALID_DATA;
  return ray;
}

Ray init_ray(vec3 o, vec3 d) {
  Ray ray;
  ray.o    = o;
  ray.d    = d;
  ray.t    = FLT_MAX;
  ray.data = RAY_INVALID_DATA;
  return ray;
}

Ray init_ray(vec3 o, vec3 d, float t_max) {
  Ray ray;
  ray.o    = o;
  ray.d    = d;
  ray.t    = t_max;
  ray.data = RAY_INVALID_DATA;
  return ray;
}

void clear(inout Ray ray) {
  ray.data = RAY_INVALID_DATA;
}

void ray_set_data_anyhit(inout Ray ray, in bool hit) { 
  ray.data = uint(hit); 
}

void ray_set_data_object(inout Ray ray, in uint object_i) {
  ray.data = RAY_OBJECT_FLAG
           | (object_i & 0x7F << 24)
           | (ray.data & 0x00FFFFFF);
}

void ray_set_data_emitter(inout Ray ray, in uint emitter_i) {
  ray.data = RAY_EMITTER_FLAG
           | (emitter_i & 0x7F << 24)
           | (ray.data & 0x00FFFFFF);
}

void ray_set_data_object_primitive(inout Ray ray, in uint primitive_i) {
  ray.data = RAY_OBJECT_FLAG
           | (ray.data & 0x7F000000)
           | (primitive_i & 0x00FFFFFF);
}

bool is_valid(in Ray ray) {
  return ray.t != FLT_MAX && ray.data != RAY_INVALID_DATA;
}

bool is_anyhit(in Ray ray) {
  return ray.data == 0x1;
}

bool hit_object(in Ray ray) {
  return (ray.data & RAY_EMITTER_FLAG) == 0;
}

bool hit_emitter(in Ray ray) {
  return (ray.data & RAY_EMITTER_FLAG) != 0;
}

/* // #define PARENS ()
// #define EXPAND(...)  EXPAND4(EXPAND4(EXPAND4(EXPAND4(__VA_ARGS__))))
// #define EXPAND4(...) EXPAND3(EXPAND3(EXPAND3(EXPAND3(__VA_ARGS__))))
// #define EXPAND3(...) EXPAND2(EXPAND2(EXPAND2(EXPAND2(__VA_ARGS__))))
// #define EXPAND2(...) EXPAND1(EXPAND1(EXPAND1(EXPAND1(__VA_ARGS__))))
// #define EXPAND1(...) __VA_ARGS__

// #define FOR_EACH(macro, ...)                                    \
//   __VA_OPT__(EXPAND(FOR_EACH_HELPER(macro, __VA_ARGS__)))
// #define FOR_EACH_HELPER(macro, a1, ...)                         \
//   macro(a1)                                                     \
//   __VA_OPT__(FOR_EACH_AGAIN PARENS (macro, __VA_ARGS__))
// #define FOR_EACH_AGAIN() FOR_EACH_HELPER

// struct PathInfoAOSPack0 { vec4 data; };
// struct PathInfoAOSPack1 { vec4 data; };
// struct PathInfoAOSPack2 { vec4 data; };
// struct PathInfoSOAPack  {
//   vec4 data_0;
//   vec4 data_1;
//   vec4 data_2;
// };

// #define SOA_ARG(dst, src, i) \
//   dst = src[i];
// #define SOA_ARG_PAIR(pair) \
//   SOA_ARG pair

// // Variadic macro to take a Type and N buffers, and to generate
// // a function that loads the type from a SOA to a AOS format
// #define ApplySOA(Ty, ...)               \
//   void soa(inout Ty ty, in uint i) {    \
//     FOR_EACH(SOA_ARG_PAIR, __VA_ARGS__) \
//   }

// #define ApplySOAInline(...) \
//   FOR_EACH(SOA_ARG_PAIR, __VA_ARGS__)

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
// } */

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