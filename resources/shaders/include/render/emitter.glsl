#ifndef RENDER_EMITTER_GLSL_GUARD
#define RENDER_EMITTER_GLSL_GUARD

#include <math.glsl>
#include <distribution.glsl>
#include <render/ray.glsl>
#include <render/frame.glsl>
#include <render/surface.glsl>
#include <render/warp.glsl>
#include <render/shape/sphere.glsl>
#include <render/shape/rectangle.glsl>
#include <render/emitter/sphere.glsl>
#include <render/emitter/rectangle.glsl>
#include <render/emitter/point.glsl>
#include <render/emitter/constant.glsl>

PositionSample sample_emitter(in EmitterInfo em, in SurfaceInfo si, in vec2 sample_2d) {
  if (em.type == EmitterTypeSphere) {
    return sample_emitter_sphere(em, si, sample_2d);
  } else if (em.type == EmitterTypeRectangle) {
    return sample_emitter_rectangle(em, si, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    return sample_emitter_point(em, si, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    return sample_emitter_constant(em, si, sample_2d);
  }
}

vec4 eval_emitter(in EmitterInfo em, in PositionSample ps, in vec4 wvls) {
  if (em.type == EmitterTypeSphere) {
    return eval_emitter_sphere(em, ps, wvls);
  } else if (em.type == EmitterTypeRectangle) {
    return eval_emitter_rectangle(em, ps, wvls);
  } else if (em.type == EmitterTypePoint) {
    return eval_emitter_point(em, ps, wvls);
  } else if (em.type == EmitterTypeConstant) {
    return eval_emitter_constant(em, wvls);
  }
}

vec4 eval_emitter(in PositionSample ps, in vec4 wvls) {
  if (!record_is_emitter(ps.data))
    return vec4(0);
  return eval_emitter(scene_emitter_info(record_get_emitter(ps.data)), ps, wvls);
}

vec4 eval_env_emitter(in vec4 wvls) {
  if (!scene_has_envm_emitter())
    return vec4(0);
  EmitterInfo em = scene_emitter_info(scene_envm_emitter_idx());
  return eval_emitter_constant(em, wvls);
}

float pdf_env_emitter(vec3 d_local) {
  if (!scene_has_envm_emitter())
    return 0.f;
  EmitterInfo em = scene_emitter_info(scene_envm_emitter_idx());
  return pdf_emitter_constant(em, d_local);
}

float pdf_emitter(in EmitterInfo em, in PositionSample ps) {
  if (em.type == EmitterTypeSphere) {
    return pdf_emitter_sphere(em, ps);
  } else if (em.type == EmitterTypeRectangle) {
    return pdf_emitter_rectangle(em, ps);
  } else if (em.type == EmitterTypePoint) {
    return pdf_emitter_point(em, ps);
  } else {
    return 0.f;
  }
}

float pdf_emitter(in PositionSample ps) {
  if (!record_is_emitter(ps.data))
    return 0.f;
  return pdf_emitter(scene_emitter_info(record_get_emitter(ps.data)), ps);
}

PositionSample sample_emitters(in SurfaceInfo si, in vec3 sample_3d) {
  // Sample emitter from distribution
  DistributionSampleDiscrete ds = sample_emitters_discrete(sample_3d.z);

  // Sample position on emitter surface
  PositionSample ps = sample_emitter(scene_emitter_info(ds.i), si, sample_3d.xy);
  record_set_emitter(ps.data, ds.i);
  ps.pdf *= ds.pdf;

  return ps;
}

float pdf_emitters(in PositionSample ps) {
  float pdf = pdf_emitter(ps);
  pdf *= pdf_emitters_discrete(record_get_emitter(ps.data));
  return pdf;
}

bool ray_intersect_emitter(inout Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;

  bool hit = false;
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.center, em.sphere_r };
    hit = ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    hit = ray_intersect(ray, em.center, em.rect_n, em.trf_inv);
  }
  
  if (hit)
    record_set_emitter(ray.data, emitter_i);

  return hit;
}

bool ray_intersect_emitter_any(in Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  
  if (!em.is_active || em.type == EmitterTypeConstant || em.type == EmitterTypePoint)
    return false;
  
  // Run intersection; on a hit, simply return
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.center, em.sphere_r };
    return ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    return ray_intersect(ray, em.center, em.rect_n, em.trf_inv);
  }
}

#endif // RENDER_EMITTER_GLSL_GUARD