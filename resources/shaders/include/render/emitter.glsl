#ifndef RENDER_EMITTER_GLSL_GUARD
#define RENDER_EMITTER_GLSL_GUARD

#include <math.glsl>
#include <distribution.glsl>
#include <render/ray.glsl>
#include <render/frame.glsl>
#include <render/sample.glsl>
#include <render/surface.glsl>
#include <render/warp.glsl>
#include <render/shape/sphere.glsl>
#include <render/shape/rectangle.glsl>
#include <render/emitter/sphere.glsl>
#include <render/emitter/rectangle.glsl>
#include <render/emitter/point.glsl>
#include <render/emitter/constant.glsl>

EmitterSample sample_emitter(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls, in vec2 sample_2d) {
  if (em.type == EmitterTypeSphere) {
    return sample_emitter_sphere(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeRectangle) {
    return sample_emitter_rectangle(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypePoint) {
    return sample_emitter_point(em, si, wvls, sample_2d);
  } else if (em.type == EmitterTypeConstant) {
    return sample_emitter_constant(em, si, wvls, sample_2d);
  }
}

vec4 eval_emitter(in EmitterInfo em, in SurfaceInfo si, in vec4 wvls) {
  if (em.type == EmitterTypeSphere) {
    return eval_emitter_sphere(em, si, wvls);
  } else if (em.type == EmitterTypeRectangle) {
    return eval_emitter_rectangle(em, si, wvls);
  } else if (em.type == EmitterTypePoint) {
    return eval_emitter_point(em, si, wvls);
  } else if (em.type == EmitterTypeConstant) {
    return eval_emitter_constant(em, wvls);
  }
}

vec4 eval_env_emitter(in vec4 wvls) {
  if (!scene_has_envm_emitter())
    return vec4(0);
  EmitterInfo em = scene_emitter_info(scene_envm_emitter_idx());
  return eval_emitter_constant(em, wvls);
}

float pdf_env_emitter(vec3 d_local, in vec4 wvls) {
  if (!scene_has_envm_emitter())
    return 0.f;
  EmitterInfo em = scene_emitter_info(scene_envm_emitter_idx());
  float pdf = pdf_emitter_constant(em, d_local);
  pdf *= pdf_emitters_discrete(scene_envm_emitter_idx());
  return pdf;
}

float pdf_emitter(in EmitterInfo em, in SurfaceInfo si) {
  if (em.type == EmitterTypeSphere) {
    return pdf_emitter_sphere(em, si);
  } else if (em.type == EmitterTypeRectangle) {
    return pdf_emitter_rectangle(em, si);
  } else if (em.type == EmitterTypePoint) {
    return pdf_emitter_point(em, si);
  } else {
    return 0.f;
  }
}

vec4 eval_emitter(in SurfaceInfo si, in vec4 wvls) {
  if (!is_emitter(si))
    return vec4(0);
  return eval_emitter(scene_emitter_info(record_get_emitter(si.data)), si, wvls);
}

float pdf_emitter(in SurfaceInfo si) {
  if (!is_emitter(si))
    return 0.f;
  return pdf_emitter(scene_emitter_info(record_get_emitter(si.data)), si);
}

EmitterSample sample_emitters(in SurfaceInfo si, in vec4 wvls, in vec3 sample_3d) {
  // Sample emitter from distribution
  DistributionSampleDiscrete ds = sample_emitters_discrete(sample_3d.z);
  
  // Sample position on emitter surface
  EmitterSample ps = sample_emitter(scene_emitter_info(ds.i), si, wvls, sample_3d.xy);
  record_set_emitter(ps.ray.data, ds.i);

  // Multiply sample pdfs
  ps.pdf *= ds.pdf;

  return ps;
}

float pdf_emitters(in SurfaceInfo si, in vec4 wvls) {
  return pdf_emitter(si) * pdf_emitters_discrete(record_get_emitter(si.data));
}

void ray_intersect_emitter(inout Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  guard(em.is_active);

  // Run intersection; flag result
  bool hit;
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.trf[3].xyz, .5f * length(em.trf[0].xyz) };
    hit = ray_intersect(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    Rectangle rectangle = { em.trf[3].xyz, em.trf[0].xyz, em.trf[1].xyz, normalize(em.trf[2].xyz) };
    hit = ray_intersect(ray, rectangle);
  } else {
    hit = false;
  }

  // Store emitter id in ray data on a closest hit
  if (hit)
    record_set_emitter(ray.data, emitter_i);
}

bool ray_intersect_emitter_any(in Ray ray, in uint emitter_i) {
  EmitterInfo em = scene_emitter_info(emitter_i);
  if (!em.is_active)
    return false;
  
  // Run intersection; on a hit, simply return
  if (em.type == EmitterTypeSphere) {
    Sphere sphere = { em.trf[3].xyz, .5f * length(em.trf[0].xyz) };
    return ray_intersect_any(ray, sphere);
  } else if (em.type == EmitterTypeRectangle) {
    Rectangle rectangle = { em.trf[3].xyz, em.trf[0].xyz, em.trf[1].xyz, normalize(em.trf[2].xyz) };
    return ray_intersect_any(ray, rectangle);
  } else {
    return false;
  }
}

#endif // RENDER_EMITTER_GLSL_GUARD