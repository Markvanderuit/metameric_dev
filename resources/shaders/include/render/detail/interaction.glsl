#ifndef GLSL_INTERACTION_DETAIL_GUARD
#define GLSL_INTERACTION_DETAIL_GUARD

#include <render/shape/primitive.glsl>

vec3 detail_gen_barycentric_coords(in vec3 p, in Primitive prim) {
  vec3 ab = prim.v1.p - prim.v0.p; 
  vec3 ac = prim.v2.p - prim.v0.p;

  float a_tri = abs(.5f * length(cross(ac,            ab           )));
  float a_ab  = abs(.5f * length(cross(p - prim.v0.p, ab           )));
  float a_ac  = abs(.5f * length(cross(ac,            p - prim.v0.p)));
  float a_bc  = abs(.5f * length(cross(prim.v2.p - p, prim.v1.p - p)));
  
  return vec3(a_bc, a_ac, a_ab) / a_tri;
}

void detail_fill_interaction_object(inout Interaction si, in Ray ray) {
  // On a valid surface, fill in surface info
  Object object_info = scene_object_info(record_get_object(si.data));
  
  // Obtain and unpack intersected primitive data
  Primitive prim = unpack(scene_blas_prim(record_get_object_primitive(ray.data)));
    
  // Reinterpolate surface position, texture coordinates, shading normal using barycentrics
  vec3 p = (inverse(object_info.trf) * vec4(ray_get_position(ray), 1)).xyz;
  vec3 b = detail_gen_barycentric_coords(p, prim);
  si.p  = b.x * prim.v0.p  + b.y * prim.v1.p  + b.z * prim.v2.p;
  si.n  = b.x * prim.v0.n  + b.y * prim.v1.n  + b.z * prim.v2.n;
  si.tx = b.x * prim.v0.tx + b.y * prim.v1.tx + b.z * prim.v2.tx;
  
  // Read normalmap data
  // vec3 nm = texture_normal(si, )

   // Compute geometric normal
  // si.ng = cross(prim.v1.p - prim.v0.p, prim.v2.p - prim.v1.p);

  // Offset surface position as shading point, as per
  // "Hacking the Shadow Terminator, Hanika, 2021"
  /* {
    vec3 tmp_u = si.p - prim.v0.p, 
         tmp_v = si.p - prim.v1.p, 
         tmp_w = si.p - prim.v2.p;
    float dot_u = min(0.f, dot(tmp_u, prim.v0.n)), 
          dot_v = min(0.f, dot(tmp_v, prim.v1.n)), 
          dot_w = min(0.f, dot(tmp_w, prim.v2.n));
          
    tmp_u -= dot_u * prim.v0.n;
    tmp_v -= dot_v * prim.v1.n;
    tmp_w -= dot_w * prim.v2.n;

    si.p += b.x * tmp_u + b.y * tmp_v + b.z * tmp_w;
  } */

  // Transform relevant data to world-space
  si.p =          (object_info.trf * vec4(si.p, 1)).xyz;
  si.n = normalize(object_info.trf * vec4(si.n, 0)).xyz;

  // Store incident ray data
  si.wi = to_local(get_frame(si.n), -ray.d);
  si.t  = ray.t;
}

void detail_fill_interaction_emitter(inout Interaction si, in Ray ray) {
  // On a valid emitter, fill in surface info
  Emitter em = scene_emitter_info(record_get_emitter(si.data));
  
  // Fill positional data from ray hit
  si.p = ray_get_position(ray);

  // Fill data based on type of area emitters
  if (em.type == EmitterTypeSphere) {
    si.n  = normalize(si.p - em.trf[3].xyz);
    si.tx = mod(vec2(atan(si.n.x, -si.n.z) * .5f, acos(si.n.y)) * M_PI_INV + 1.f, 1.f);
  } else if (em.type == EmitterTypeRectangle) {
    si.n  = normalize(em.trf[2].xyz);
    si.tx = 0.5 + (inverse(em.trf) * vec4(si.p, 1)).xy;
  } else if (em.type == EmitterTypePoint || em.type == EmitterTypeConstant) {
    si.n  = ray.d;
    si.tx = mod(vec2(atan(si.n.x, -si.n.z) * .5f, acos(si.n.y)) * M_PI_INV + 1.f, 1.f);
  }

  // Generate shading frame based on geometric normal
  si.wi = to_local(get_frame(si.n), -ray.d);
  si.t  = ray.t;
}

#endif // GLSL_INTERACTION_DETAIL_GUARD